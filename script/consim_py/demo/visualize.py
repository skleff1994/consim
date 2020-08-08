import numpy as np 
import pinocchio as pin 
import consim 
import os, sys
import matplotlib.pyplot as plt 
from consim_py.utils import visualize
from consim_py.utils.visualize  import getModelPath, getVisualPath 
from os.path import dirname, exists, join



    
if  __name__=="__main__":
    URDF_FILENAME = "solo12.urdf"
    SRDF_FILENAME = "solo.srdf"
    SRDF_SUBPATH = "/solo_description/srdf/" + SRDF_FILENAME
    URDF_SUBPATH = "/solo_description/robots/" + URDF_FILENAME
    modelPath = getModelPath(URDF_SUBPATH)


    visualizer = visualize.Visualizer()
    viz_object = visualize.ConsimVisual("solo-Euler", modelPath + URDF_SUBPATH, 
                        [getVisualPath(modelPath)], pin.JointModelFreeFlyer(), 
                        visualizer.viewer, visualizer.sceneName)
    #
    q = np.zeros(viz_object.model.nq)
    q[6] = 1 # quaternion 
    q[2] = 1. 
    viz_object.loadViewerModel()
    viz_object.display(q)
    #
    hyq_urdf_file = "hyq_no_sensors.urdf"
    hyq_srdf_file = "hyq.srdf"
    HYQ_URDF_SUBPATH = "/hyq_description/robots/" + hyq_urdf_file
    HYQ_SRDF_SUBPATH = "/hyq_description/srdf/" + hyq_srdf_file
    hyq_modelPath = getModelPath(URDF_SUBPATH)

    viz_object2 = visualize.ConsimVisual("hyq-Exp", hyq_modelPath + HYQ_URDF_SUBPATH, 
                        [getVisualPath(hyq_modelPath)], pin.JointModelFreeFlyer(), 
                        visualizer.viewer, visualizer.sceneName)
    #
    q2 = np.zeros(viz_object2.model.nq)
    q2[6] = 1 # quaternion 
    q2[1] = 1.
    q2[2] = 1. 
    viz_object2.loadViewerModel()
    viz_object2.display(q2)

