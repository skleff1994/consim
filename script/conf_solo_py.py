# -*- coding: utf-8 -*-
"""
Created on Thu Apr 18 09:47:07 2019

@author: student
"""

import numpy as np
import os

np.set_printoptions(precision=3, linewidth=200, suppress=True)
LINE_WIDTH = 60

#N_SIMULATION = 4000             # number of time steps simulated
#dt = 0.002                      # controller time step

mu = 0.3                            # friction coefficient
fMin = 1.0                          # minimum normal force
fMax = 1000.0                       # maximum normal force
contact_frames = ['BL_contact', 'BR_contact', 'FL_contact', 'FR_contact']
contact_normal = np.matrix([0., 0., 1.]).T   # direction of the normal to the contact surface
K = 1e5*np.asmatrix(np.diagflat([1., 1., 1.]))
B = 3e2*np.asmatrix(np.diagflat([1., 1., 1.]))

w_com = 1.0                     # weight of center of mass task
w_posture = 1e-3                # weight of joint posture task
w_forceRef = 1e-6               # weight of force regularization task

kp_contact = 10.0               # proportional gain of contact constraint
kp_com = 10.0                   # proportional gain of center of mass task
kp_posture = 10.0               # proportional gain of joint posture task

PRINT_T = 0.2                   # print every PRINT_T
DISPLAY_T = 0.1                 # update robot configuration in viwewer every DISPLAY_T
T = 2.0

amp        = np.matrix([0.0, 0.02, 0.0]).T
two_pi_f   = 2*np.pi*np.matrix([0.0, .6, 0.0]).T

#filename = str(os.path.dirname(os.path.abspath(__file__)))
#path = filename + '/../models'
path = '/home/student/devel/src/tsid/models'
urdf = path + '/quadruped/urdf/quadruped.urdf'
q0 = np.matrix([[0., 0., 0.223, 0., 0., 0., 1., 
                 -0.8,  1.6, -0.8, 1.6, 
                 -0.8,  1.6, -0.8, 1.6]]).T

use_viewer = 1
CAMERA_TRANSFORM = [2.0044965744018555, 0.9386290907859802, 0.9415794014930725, 
                    0.3012915551662445, 0.49565795063972473, 0.6749107837677002, 0.45611628890037537]

SPHERE_RADIUS = 0.03
REF_SPHERE_RADIUS = 0.03
COM_SPHERE_COLOR  = (1, 0.5, 0, 1)
COM_REF_SPHERE_COLOR  = (1, 0, 0, 1)
RF_SPHERE_COLOR  = (0, 1, 0, 1)
RF_REF_SPHERE_COLOR  = (0, 1, 0.5, 1)
LF_SPHERE_COLOR  = (0, 0, 1, 1)
LF_REF_SPHERE_COLOR  = (0.5, 0, 1, 1)
