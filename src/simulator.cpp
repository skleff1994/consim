#include <pinocchio/spatial/se3.hpp>
#include <pinocchio/multibody/model.hpp>
#include <pinocchio/algorithm/aba.hpp>
#include <pinocchio/algorithm/joint-configuration.hpp> // se3.integrate
#include <pinocchio/algorithm/frames.hpp>
#include <pinocchio/algorithm/contact-dynamics.hpp>
#include <pinocchio/algorithm/compute-all-terms.hpp>
#include <pinocchio/algorithm/kinematics.hpp>

#include "consim/object.hpp"
#include "consim/contact.hpp"
#include "consim/simulator.hpp"
// #include "consim/utils/stop-watch.hpp"



#include <iostream>


namespace consim {

/** 
 * AbstractSimulator Class 
*/

AbstractSimulator::AbstractSimulator(const pinocchio::Model &model, pinocchio::Data &data, float dt, int n_integration_steps): 
model_(&model), data_(&data), dt_(dt), n_integration_steps_(n_integration_steps) {
  
  q_.resize(model.nq);
  dq_.resize(model.nv);
  ddq_.resize(model.nv);
  dqMean_.resize(model.nv);
  tau_.resize(model.nv);
  frame_Jc_.resize(3, model.nv);
  J_.resize(6, model.nv);
} 

const ContactPoint &AbstractSimulator::addContactPoint(int frame_id)
{
  ContactPoint *cptr = new ContactPoint();
  cptr->active = false;
  cptr->f.fill(0);
  cptr->friction_flag = false;
  cptr->frame_id = frame_id;
	contacts_.push_back(cptr);
  nc_ += 1; // increase contact points count  
  nk_ = 3*nc_;
  return getContact(contacts_.size() - 1);
}

const ContactPoint &AbstractSimulator::getContact(int index)
{
  return *contacts_[index];
}

void AbstractSimulator::addObject(Object& obj) {
  objects_.push_back(&obj);
}

void AbstractSimulator::resetState(const Eigen::VectorXd& q, const Eigen::VectorXd& dq, bool reset_contact_state)
{
  q_ = q;
  dq_ = dq;
  dqMean_ = dq;

  if (reset_contact_state) {
    for (auto &cptr : contacts_) {
      cptr->active = false;
      cptr->f.fill(0);
      cptr->friction_flag = false;
    }
  }

  computeContactState();
  computeContactForces(dq_);
  resetflag_ = true;
}

void AbstractSimulator::checkContact()
{
  nactive_ = 0; 
  // Loop over all the contact points, over all the objects.
  for (auto &cp : contacts_) {
    cp->x = data_->oMf[cp->frame_id].translation();
    if (cp->active) {
      // optr: object pointer
      if (!cp->optr->checkContact(*cp))
      {
        cp->active = false;
        cp->f.fill(0);
        cp->friction_flag = false;
      } else {
        nactive_ += 1;
        // If the contact point is still active, then no need to search for
        // other contacting object (we assume there is only one object acting
        // on a contact point at each timestep).
        continue;
      }
    }

    for (auto &optr : objects_) {
      if (optr->checkContact(*cp))
      {
        cp->active = true;
        nactive_ += 1; 
        cp->optr = optr;
        break;
      }
    }
  }
}

void AbstractSimulator::computeContactState()
{
  tau_.fill(0);

  // Compute all the terms (mass matrix, jacobians, ...)
  data_->M.fill(0);
  getProfiler().start("pinocchio::computeAllTerms");
  pinocchio::computeAllTerms(*model_, *data_, q_, dq_);
  pinocchio::updateFramePlacements(*model_, *data_);
  getProfiler().stop("pinocchio::computeAllTerms");

  // Contact handling: Detect contact, compute contact forces, compute resulting torques.
  getProfiler().start("check_contact_state");
  checkContact();
  // computeContactForces(dq_);
  getProfiler().stop("check_contact_state");
}

void AbstractSimulator::setJointFriction(const Eigen::VectorXd& joint_friction)
{
  joint_friction_flag_=1;
  joint_friction_ = joint_friction;
}

inline void AbstractSimulator::contactLinearJacobian(unsigned int frame_id)
{
  J_.setZero();
  pinocchio::getFrameJacobian(*model_, *data_, frame_id, pinocchio::LOCAL_WORLD_ALIGNED, J_);
  frame_Jc_ = J_.topRows(3);
}

/* ____________________________________________________________________________________________*/
/** 
 * EulerSimulator Class 
*/

EulerSimulator::EulerSimulator(const pinocchio::Model &model, pinocchio::Data &data, float dt, int n_integration_steps):
AbstractSimulator(model, data, dt, n_integration_steps), sub_dt(dt / ((double)n_integration_steps)) {

}

void EulerSimulator::computeContactForces(const Eigen::VectorXd &dq) 
{
  getProfiler().start("compute_contact_forces");
  // subtract joint frictions
  if (joint_friction_flag_){
    tau_ -= joint_friction_.cwiseProduct(dq);
  }

  for (auto &cp : contacts_) {
    if (!cp->active) continue;

    // If the contact point is active, compute it's velocity and call the
    // contact model function on the object.
    // TODO: Is there a faster way to compute the contact point velocity than
    //       multiply the jacobian with the generalized velocity from pinocchio?
    contactLinearJacobian(cp->frame_id);
    cp->v = frame_Jc_ * dq;
    // printf("contact point position %f\n", cp->normal(2));
    // printf("contact point velocity %f\n", cp->normvel(2));
    // contact force computation is called in th
    cp->optr->contactModel(*cp);
    // printf("contact point force %f\n", cp->f(2));

    tau_ += frame_Jc_.transpose() * cp->f;
    // printf("integration force %f\n", tau_(2));
  }
  getProfiler().stop("compute_contact_forces");
}


void EulerSimulator::step(const Eigen::VectorXd &tau) 
{
  getProfiler().start("euler_simulator::step");
  assert(tau.size() == model_->nv);
  for (int i = 0; i < n_integration_steps_; i++)
    {
      // TODO: Support friction models at the joints.

      // Add the user torque;
      tau_ += tau;

      // Compute the acceloration ddq.
      getProfiler().start("pinocchio::aba");
      pinocchio::aba(*model_, *data_, q_, dq_, tau_);
      getProfiler().stop("pinocchio::aba");

      // Integrate the system forward in time.
      dqMean_ = dq_ + data_->ddq * .5 * sub_dt;
      q_ = pinocchio::integrate(*model_, q_, dqMean_ * sub_dt);
      dq_ += data_->ddq * sub_dt;

      // Compute the new data values and contact information after the integration
      // step. This way, if this method returns, the values computed in data and
      // on the contact state are consistent with the q, dq and ddq values.
      computeContactState();
      computeContactForces(dq_);
    }
  getProfiler().stop("euler_simulator::step");
}





/* ____________________________________________________________________________________________*/
/** 
 * ExponentialSimulator Class 
*/

ExponentialSimulator::ExponentialSimulator(const pinocchio::Model &model, pinocchio::Data &data, float dt, int n_integration_steps,
                                           bool sparse, bool invertibleA) : AbstractSimulator(model, data, dt, n_integration_steps), 
                                           sparse_(sparse), invertibleA_(invertibleA), sub_dt(dt / ((double)n_integration_steps))
{
  dJvi_.resize(3);
  ddqMean_.resize(model_->nv);
  Minv_.resize(model_->nv, model_->nv); Minv_.setZero();
  dv0_.resize(model_->nv); dv0_.setZero();
  // crossAcci_.resize(3); 
  // ai_.resize(3);
  // dJvilocal_.resize(3);
  // ailocal_.resize(3);
}

void ExponentialSimulator::allocateData(){
  // this function must be called after adding all contacts 
  f_.resize(nk_); 
  p0_.resize(nk_); 
  p_.resize(nk_); 
  dp_.resize(nk_); 
  Jc_.resize(nk_, model_->nv); Jc_.setZero();
  dJv_.resize(nk_); dJv_.setZero();
  a_.resize(2 * nk_); a_.setZero();
  A.resize(2 * nk_, 2 * nk_); A.setZero();
  K.resize(nk_, nk_); K.setZero();
  B.resize(nk_, nk_); B.setZero();
  D.resize(2*nk_, nk_); D.setZero();
  xt_.resize(nk_);
  intxt_.resize(nk_); 
  int2xt_.resize(nk_);
  
} //ExponentialSimulator::allocateData


void ExponentialSimulator::step(const Eigen::VectorXd &tau){
  if(!resetflag_){
    throw std::runtime_error("resetState() must be called first !");
  }
  tau_ += tau; 
  if (nactive_> 0){
  // compute Kp0_
  kp0_ = K*p0_;
  // compute Minv & dv0_
  Minv_ = pinocchio::computeMinverse(*model_, *data_, q_);
  JMinv_ = Jc_ * Minv_;
  dv0_ = Minv_ * (tau_ - data_->nle + Jc_.transpose() * kp0_);
  // fill out A matrix
  Upsilon_ = JMinv_ * Jc_.transpose();
  A.block(0, 3*nactive_, 3*nactive_, 3*nactive_) = Eigen::MatrixXd::Identity(3*nactive_, 3*nactive_);
  A.block(3*nactive_, 0, 3*nactive_, 3*nactive_) = -Upsilon_ * K;
  A.block(3*nactive_, 3*nactive_, 3*nactive_, 3*nactive_) = -Upsilon_ * B;
  b_ = JMinv_ * (tau_ - data_->nle) + dJv_ + Upsilon_*kp0_;
  // stack x0 
  x0_.segment(0, 3*nactive_) = p_; 
  x0_.segment(3*nactive_, 3*nactive_) = dp_; 
  //
  if (sparse_)
  {
    throw std::runtime_error("Sparse integration not implemented yet");
  } // sparse 
  else{
    if(invertibleA_){
      throw std::runtime_error("Invertible and dense integration not implemented yet");
    } //invertible dense 
    else{
      printf("solving dense system started\n");
      solveDenseExpSystem();
      printf("solving dense system ended\n");
    } // non-invertable dense
  }
  // do the integration 
  dqMean_ = dq_ + .5 * dt_ * dv0_ + JMinv_.transpose()*D*int2xt_/dt_ ;
  ddqMean_ = dv0_ + JMinv_.transpose()*D*intxt_/dt_ ;
  dq_ += dt_*ddqMean_;
  q_ = pinocchio::integrate(*model_, q_, dqMean_ * dt_);
  ddq_ = ddqMean_; 
  } // active contacts > 0 
  else{
    pinocchio::aba(*model_, *data_, q_, dq_, tau_);
    ddq_ = data_->ddq; 
    dqMean_ = dq_ + ddq_ * .5 * dt_;
    q_ = pinocchio::integrate(*model_, q_, dqMean_ * dt_);
    dq_ += data_->ddq * dt_; 
  }
  pinocchio::forwardKinematics(*model_, *data_, q_, dq_, ddq_); 
  computeContactState();
  computeContactForces(dq_);
} // ExponentialSimulator::step



  void ExponentialSimulator::computeContactForces(const Eigen::VectorXd &dq)
{
  resizeVectorsAndMatrices();
  // tau_ was already set to zero in checkContactStates
  if (joint_friction_flag_)
  {
    tau_ -= joint_friction_.cwiseProduct(dq);
  }
  // loop over contacts, compute the force and store in vector f 
  for(unsigned int i=0; i<nc_; i++){
    if (!contacts_[i]->active) continue;
    // compute jacobian for active contact and store inn frame_Jc_
    contactLinearJacobian(contacts_[i]->frame_id);
    Jc_.block(3*i,0,3,model_->nv) = frame_Jc_;
    contacts_[i]->v = frame_Jc_ * dq;
    p0_.segment(3*i,3)=contacts_[i]->x_start; 
    p_.segment(3*i,3)=contacts_[i]->x; 
    dp_.segment(3*i,3)=contacts_[i]->v; 
    // compute force using the model  
    contacts_[i]->optr->contactModel(*contacts_[i]);
    f_.segment(3*i,3) = contacts_[i]->f; 
    // fill out K&B
    K(3*i, 3*i) = contacts_[i]->optr->getTangentialStiffness();
    K(3*i + 1, 3*i + 1) = contacts_[i]->optr->getTangentialStiffness();
    K(3*i + 2, 3 * i + 2) = contacts_[i]->optr->getNormalStiffness();
    B(3*i, 3*i) = contacts_[i]->optr->getTangentialDamping();
    B(3*i + 1, 3*i + 1) = contacts_[i]->optr->getTangentialDamping();
    B(3*i + 2, 3*i + 2) = contacts_[i]->optr->getNormalDamping();
    // compute dJvi_
    computeFrameAcceleration(contacts_[i]->frame_id); 
    dJv_.segment(3*i,3) = dJvi_; 
  }

  D.block(0,0, 3*nactive_, 3*nactive_) = K;
  D.block(0,3*nactive_, 3*nactive_, 3*nactive_) = B; 
} // ExponentialSimulator::computeContactForces

void ExponentialSimulator::computeFrameAcceleration(unsigned int frame_id)
{
  dJvi_.setZero();
  vilocal_ = pinocchio::getFrameVelocity(*model_, *data_, frame_id);
  dJvilocal_ = pinocchio::getFrameAcceleration(*model_, *data_, frame_id);
  dJvilocal_.linear() += vilocal_.angular().cross(vilocal_.linear());
  frameSE3_.rotation() = data_->oMf[frame_id].rotation();
  dJvi_ = frameSE3_.act(dJvilocal_).linear();
} //computeFrameAcceleration


void ExponentialSimulator::solveDenseExpSystem()
{
  a_.segment(nactive_, nactive_) = b_;
  // xt_ = utilDense_.ComputeXt(A, a_, x0_, dt_);
  utilDense_.ComputeIntegralXt(A, a_, x0_, dt_, intxt_);
  utilDense_.ComputeDoubleIntegralXt(A, a_, x0_, dt_, int2xt_); 
} // ExponentialSimulator::solveDenseExpSystem

void ExponentialSimulator::solveSparseExpSystem()
{

} // ExponentialSimulator::solveSparseExpSystem

void ExponentialSimulator::resizeVectorsAndMatrices()
{
  // Operations below need optimization, this is a first attempt
  // resize matrices and fillout contact information
  // TODO: change to use templated header dynamic_algebra.hpp
  f_.resize(3 * nactive_); f_.setZero();
  p0_.resize(3 * nactive_); p0_.setZero();
  p_.resize(3 * nactive_); p_.setZero();
  dp_.resize(3 * nactive_); dp_.setZero();
  a_.resize(3 * nactive_); a_.setZero();
  b_.resize(3 * nactive_); b_.setZero();
  x0_.resize(6 * nactive_); x0_.setZero();
  xt_.resize(6 * nactive_); xt_.setZero();
  intxt_.resize(6 * nactive_); intxt_.setZero();
  int2xt_.resize(6 * nactive_); int2xt_.setZero();
  kp0_.resize(3 * nactive_); kp0_.setZero();
  K.resize(3 * nactive_, 3 * nactive_); K.setZero();
  B.resize(3 * nactive_, 3 * nactive_); B.setZero();
  D.resize(3 * nactive_, 6 * nactive_); D.setZero();
  A.resize(6 * nactive_, 6 * nactive_); A.setZero();
  Jc_.resize(3 * nactive_, model_->nv); Jc_.setZero();
  Upsilon_.resize(3 * nactive_, 3 * nactive_); Upsilon_.setZero();
  JMinv_.resize(3 * nactive_, model_->nv); JMinv_.setZero();
  dJv_.resize(3 * nactive_); dJv_.setZero();
  utilDense_.resize(6 * nactive_);
} // ExponentialSimulator::resizeVectorsAndMatrices

/* ____________________________________________________________________________________________*/



}  // namespace consim 
