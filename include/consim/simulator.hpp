#pragma once

#include <Eigen/Eigen>
#include<Eigen/Cholesky>
#include <pinocchio/spatial/se3.hpp>
#include <pinocchio/multibody/model.hpp>
#include <pinocchio/multibody/data.hpp>
#include <pinocchio/spatial/motion.hpp>

#include <MatrixExponential.hpp>
#include <LDSUtility.hpp>

#include "consim/object.hpp"
#include "consim/contact.hpp"
#include "eiquadprog/eiquadprog-fast.hpp"

#define CONSIM_PROFILER
#ifndef CONSIM_PROFILER
#define CONSIM_START_PROFILER(name)
#define CONSIM_STOP_PROFILER(name)
#else
#define CONSIM_START_PROFILER(name) getProfiler().start(name)
#define CONSIM_STOP_PROFILER(name) getProfiler().stop(name)
#endif





namespace consim {

  class AbstractSimulator {
    public:
      AbstractSimulator(const pinocchio::Model &model, pinocchio::Data &data, float dt, int n_integration_steps); 
      ~AbstractSimulator(){};

      /**
        * Defines a pinocchio frame as a contact point for contact interaction checking.
        * A contact is a struct containing all the contact information 
      */
      const ContactPoint &addContactPoint(std::string name, int frame_id, bool unilateral);

      /**
        * Returns the contact points reference 
      */

      const ContactPoint &getContact(std::string name);

      /**
        * Adds an object to the simulator for contact interaction checking.
      */
      void addObject(ContactObject &obj);

      /**
       * Convenience method to perform a single dt timestep of the simulation. The
       * q and dq after the step are available form the sim.q and sim.dq property.
       * The acceloration during the last step is available from data.ddq;
       * 
       * The jacobian, frames etc. of data is update after the final q/dq values
       * are computed. This allows to use the data object after calling step
       * without the need to re-run the computeXXX methods etc.
       */

      void resetState(const Eigen::VectorXd &q, const Eigen::VectorXd &dq, bool reset_contact_state);

      void setJointFriction(const Eigen::VectorXd &joint_friction);

      /**
       * Convenience method to perform a single dt timestep of the simulation. 
       * Computes q, dq, ddq, and contact forces for a single time step 
       * results are stored in sim.q, sim.dq, sim.ddq, sim.f 
       */

      virtual void step(const Eigen::VectorXd &tau)=0;

      const Eigen::VectorXd& get_q() const {return q_;};
      const Eigen::VectorXd& get_v() const {return v_;};
      const Eigen::VectorXd& get_dv() const {return dv_;};

    protected:
      const double sub_dt;
      Eigen::VectorXd q_;  
      Eigen::VectorXd qnext_;
      Eigen::VectorXd v_;
      Eigen::VectorXd dv_;
      Eigen::VectorXd vMean_;
      Eigen::VectorXd tau_;
      unsigned int nc_=0;
      int nactive_; 
      bool resetflag_ = false;
      /**
        * loops over contact points, checks active contacts and sets reference contact positions 
      */
      void detectContacts();
      virtual void computeContactForces()=0;

      const pinocchio::Model *model_;
      pinocchio::Data *data_;

      double dt_;
      int n_integration_steps_;
  
      std::vector<ContactPoint *> contacts_;
      std::vector<ContactObject *> objects_;

      Eigen::VectorXd joint_friction_;
      bool joint_friction_flag_ = 0;
  
  }; // class AbstractSimulator

/*_______________________________________________________________________________*/


  class EulerSimulator : public AbstractSimulator
  {
    public: 
      EulerSimulator(const pinocchio::Model &model, pinocchio::Data &data, float dt, int n_integration_steps); 
      ~EulerSimulator(){};

    /**
     * Explicit Euler first oder step 
    */

      void step(const Eigen::VectorXd &tau) override;

    protected:
      void computeContactForces() override;

  }; // class EulerSimulator

/*_______________________________________________________________________________*/

  class ExponentialSimulator : public AbstractSimulator
  {
    public:
      ExponentialSimulator(const pinocchio::Model &model, pinocchio::Data &data, float dt, int n_integration_steps, 
                          bool sparse=false, bool invertibleA=false); 
      ~ExponentialSimulator(){};
      void step(const Eigen::VectorXd &tau) override;

    protected:
      /**
       * AbstractSimulator::computeContactState() must be called before  
       * calling ExponentialSimulator::computeContactForces()
       */
      void computeContactForces() override; 
      /**
       * computes average contact force during one integration step 
       * loops over the average force to compute tangential and normal force per contact 
       * projects any violations of the cone onto its boundaries 
       * sets a flag to to switch integration mode to include saturated forces 
       */
      void checkFrictionCone(); 

      void resizeVectorsAndMatrices();
      // convenience method to compute terms needed in integration  
      void computeIntegrationTerms();

      bool sparse_; 
      bool invertibleA_;
      
      Eigen::VectorXd f_;  // total force 
      Eigen::MatrixXd Jc_; // contact jacobian for all contacts 
      Eigen::VectorXd p0_; // reference position for contact 
      Eigen::VectorXd p_; // current contact position 
      Eigen::VectorXd dp_; // contact velocity
      Eigen::VectorXd x0_;
      Eigen::VectorXd a_;
      Eigen::VectorXd b_;
      Eigen::VectorXd xt_;  // containts p and dp for all active contact points 
      Eigen::VectorXd intxt_;
      Eigen::VectorXd int2xt_;
      Eigen::VectorXd kp0_; 
      Eigen::VectorXd dv_bar; 
      // contact acceleration components 
      Eigen::VectorXd dJv_;  
      Eigen::MatrixXd K;
      Eigen::MatrixXd B;
      Eigen::MatrixXd D;
      Eigen::MatrixXd A; 
      Eigen::MatrixXd Minv_;
      Eigen::MatrixXd JMinv_;
      Eigen::MatrixXd MinvJcT_;
      Eigen::MatrixXd Upsilon_;
      Eigen::MatrixXd JcT_; 
      // expokit 
      expokit::LDSUtility<double, Dynamic> utilDense_;
      // 
      Eigen::VectorXd dvMean_;
      Eigen::VectorXd temp01_;
      Eigen::VectorXd temp02_;
      Eigen::VectorXd temp03_;
      Eigen::VectorXd temp04_;
      Eigen::MatrixXd tempStepMat_; 
      // friction cone 
      Eigen::VectorXd f_avg;  // average force for cone 
      Eigen::VectorXd fpr_;   // projected force on cone boundaries 
      bool cone_flag_ = false; // cone violation status 
      double cone_direction_; // angle of tangential(to contact surface) force 

      Eigen::Vector3d normalFi_; // normal component of contact force Fi at contact Ci  
      Eigen::Vector3d tangentFi_; // normal component of contact force Fi at contact Ci  
      double fnor_;   // norm of normalFi_  
      double ftan_;   // norm of tangentFi_ 
      unsigned int i_active_; // index of the active contact      

      /**
       * solves a QP to update anchor points of sliding contacts
       * min || dp0_avg || ^ 2 
       * st. Fc \in Firction Cone
       **/  
      void computeSlipping(); 
      eiquadprog::solvers::EiquadprogFast qp;
      Eigen::MatrixXd Q_cone; 
      Eigen::VectorXd q_cone; 
      Eigen::MatrixXd Cineq_cone; 
      Eigen::VectorXd cineq_cone; 
      Eigen::MatrixXd Ceq_cone; 
      Eigen::VectorXd ceq_cone; 
      Eigen::VectorXd optdP_cone; 
      //
      Eigen::Vector3d xstart_new; // invK*cone_force_offset_03

      Eigen::MatrixXd normal_constraints_;
      Eigen::MatrixXd tangentA_constraints_;
      Eigen::MatrixXd tangentB_constraints_;
      Eigen::MatrixXd contact_position_integrator_; 
      Eigen::MatrixXd D_intExpA_integrator; 

      void computePredictedForces();
      Eigen::MatrixXd C; 
      Eigen::VectorXd z; 
      Eigen::VectorXd nextZ; 
      Eigen::MatrixXd expDtC;
      Eigen::VectorXd predictedForce_;  
      expokit::MatrixExponential<double, Dynamic> utilD; // Dynamic
  }; // class ExponentialSimulator

} // namespace consim 
