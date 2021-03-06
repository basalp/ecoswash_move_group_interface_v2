  /* Modification for ecoSwash: EB, PH */

  #include "move_group_interface_left_lidar.h" // #include "../include/move_group_interface_left_lidar.h"
  #include "moveit_interface.h"

  // =============================================================================================================
  // Variable declatation
  // =============================================================================================================

  // unit conversion
  const double RAD2DEG = 180/M_PI;
  const double DEG2RAD = M_PI/180;

  // direction enumeration
  const double ROT_CW  = -1.0;
  const double ROT_CCW =  1.0;

  // MIN - MAX Joint Values  // to be received from ros params later
  const double MIN_J1 = 0.7; // [m]
  const double MAX_J1 = 7.4; // [m] 

  double j1_tol_below;
  double j1_tol_above;

  // Define 3 quaternion for original, the desired rotation we'd like to command, and the new orientation, respectively. 
  tf2::Quaternion q_orig, q_des_rot, q_new_rot;

  // count number for rotations (for logging purpose)
  int rot_c_eef = 0;

  // define the desired orientation in RPY (Roll wrt world frame X. Pitch wrt wf Y, Yaw wrt Z)
  double roll  = 0.0 * DEG2RAD;  // X
  double pitch = 0.0 * DEG2RAD;  // Y
  double yaw   = 0.0 * DEG2RAD;  // Z  

  // distance [m] to command the eef to move through PoseStamped
  double move_rel_pose_x = 0.0;
  double move_rel_pose_y = 0.0;
  double move_rel_pose_z = 0.0;

  // absolute target value to calculate the relative distance to move the end effector
  double abs_posegoal_x = 0;
  double abs_posegoal_y = 0;
  double abs_posegoal_z = 0;

  // flag to set whether the move_pose is relative (1) to current pose or an absolute (0)
  bool is_relative = 1; // by default keep it at relative as the inital pose move displacements are 0

  // plan success
  bool success_plan = 0;

  // parameters related rotation of toolhead in a certain plane
  int pos_yaw_rot_count = 0;
  int neg_yaw_rot_count = 0;

  // step angle to rotate the end effector at a xyz pose
  double step_roll_deg  = 15.0; // change the Roll rotation by 10degrees
  double step_pitch_deg = 15.0; // change the Pitch rotation by 10degrees
  double step_yaw_deg   = 15.0; // change the Yaw rotation by 10degrees

  // max absolute angle to rotate the end effector at a xyz pose
  double max_abs_roll  = 35.00;
  double max_abs_pitch = 35.00;
  double max_abs_yaw   = 35.00;

	// !! this implementation to be changed later !!
	double cur_roll_ang  = 0;
	double cur_pitch_ang = 0;
	double cur_yaw_ang   = 0;


  // Pose messages
  geometry_msgs::PoseStamped pose_stm;

  // Publisher declaration
  ros::Publisher left_robot_eef; // for now, defined globally to be used in helper functions out of main()



  // !! TRIAL !!
/* Note about boost::shared_ptr
   If the functions take a shared pointer, it should be because they need to extend the lifetime of the object. 
   If they don't need to extend the lifetime of the object, they should take a reference. 
*/
  typedef boost::shared_ptr < ros::Publisher > left_rob_pose_ptr;



  // ******************************

/* MoveGroupInterfaceLeftLidar::MoveGroupInterfaceLeftLidar(){}
  MoveGroupInterfaceLeftLidar::~MoveGroupInterfaceLeftLidar(){}

  MoveGroupInterfaceLeftLidar mgi;
*/


  // =============================================================================================================
  // Helper functions
  // =============================================================================================================

  // ******************************
  /* Function to log joint positions in degree (except linear axis[m]) */
  void log_joint_positions(std::vector<double> &joint_group_pos)
  {
    std::vector<double> joint_group_pos_deg = joint_group_pos;

    std::transform(joint_group_pos.begin()+1, joint_group_pos.end(), 
                  joint_group_pos_deg.begin()+1, [&](double &joint_i){ return RAD2DEG*joint_i; }); //{ return ::RAD2DEG*joint_i; });

    // print the joint_group_positions for each joint before assignment (in Degree)
    ROS_INFO("Joint Positions ( linear axis[m], rotational joints[deg]: ) \n");
    std::copy(joint_group_pos_deg.begin(), joint_group_pos_deg.end(), std::ostream_iterator<double>(std::cout, ", "));                  
  }

  // ******************************
  /* Function to receive the desired RPY values and move the end effector accordingly */
  int rotate_eef_through_rpy_set(moveit::planning_interface::MoveGroupInterface& move_grp, 
                                 moveit::planning_interface::MoveGroupInterface::Plan &m_plan, 
                                 tf2::Quaternion q_org, tf2::Quaternion q_des, tf2::Quaternion q_new, 
                                 double roll_a, double pitch_a, double yaw_a, std::string eef_name, int rot_eef_c)
  {  
    pose_stm = move_grp.getCurrentPose(eef_name);

    // Get the current orientation of pose to be further oriented/controlled
    tf2::convert(pose_stm.pose.orientation , q_org);
    
    // set the desired orientation in RPY & calculate the new orientation & normalize
    q_des.setRPY(roll_a, pitch_a, yaw_a);
    q_new = q_des*q_org;  
    q_new.normalize();
    // empose the new orientation back into the pose (requires conversion into a msg type)
    tf2::convert(q_new, pose_stm.pose.orientation);

    rot_eef_c++;

    ROS_INFO_STREAM("Target Pose after rotatiton: " << rot_eef_c << "\n" << pose_stm << "\n");
    move_grp.setPoseTarget(pose_stm);
    if( (move_grp.plan(m_plan) == moveit::planning_interface::MoveItErrorCode::SUCCESS) ) { move_grp.execute(m_plan); }  

    // check if the set pose was really set & publish the new pose after execution of the robot
    pose_stm = move_grp.getCurrentPose(eef_name);
    left_robot_eef.publish(pose_stm);  

    return rot_eef_c;
  }

  // ******************************
  /* Function to receive the desired RPY values and move the end effector accordingly */
  void set_eef_xyz_pose(moveit::planning_interface::MoveGroupInterface& move_grp, moveit::planning_interface::MoveGroupInterface::Plan &m_plan, 
                        double x_d, double y_d, double z_d, std::string eef_name, bool flag_relative)                          
  {  
    pose_stm = move_grp.getCurrentPose(eef_name);
    
    if(flag_relative){
    // if desired distance command is relative to current pose
    pose_stm.pose.position.x += x_d;  
    pose_stm.pose.position.y += y_d; 
    pose_stm.pose.position.z += z_d;  
  } 
  else{
    // if desired pose is an absolute pose
    pose_stm.pose.position.x = x_d;  
    pose_stm.pose.position.y = y_d; 
    pose_stm.pose.position.z = z_d;  
  }

    // Set the entered target pose
    move_grp.setPoseTarget(pose_stm); // previously done with converting the PoseStamped to Pose but that's not necessary
    ROS_INFO_STREAM("Current pose on the tour path: \n" << pose_stm << "\n");

    success_plan = (move_grp.plan(m_plan) == moveit::planning_interface::MoveItErrorCode::SUCCESS);
    if(success_plan) { move_grp.execute(m_plan); }

    // check if the set pose was really set & publish the new pose after execution of the robot
    pose_stm = move_grp.getCurrentPose(eef_name);
    left_robot_eef.publish(pose_stm); 
 
  }

  // ******************************


  // =============================================================================================================
  // Main Function
  // =============================================================================================================

int main(int argc, char** argv)
{
  ros::init(argc, argv, "ecos_move_group_interface");
  ros::NodeHandle node_handle;

  // below line it the working one for now
  left_robot_eef = node_handle.advertise<geometry_msgs::PoseStamped>("left_robot_eef_pose", 1000);


  /*ros::Publisher left_robot_eef = node_handle.advertise<geometry_msgs::PoseStamped>("left_robot_eef_pose", 1000);
  left_rob_pose_ptr left_rob(new ros::Publisher); */


  ros::Rate rate(10);

  ros::AsyncSpinner spinner(2); // since there are 2 move_group nodes, this is  - I assume - was necessary. Eventually to be dropped to single node again
  spinner.start();


  // =============================================================================================================
  // Setup
  // =============================================================================================================
 
  // MOVEIT INTERFACE OBJECT
  ESMoveItInterface robot_l;


// TO BE DELETED LATER START
  static const std::string PLANNING_GROUP_LEFT = "left_staubli";
  // the :move_group_interface:MoveGroupInterface class can be setup using the name of the planning group to control/plan.
  moveit::planning_interface::MoveGroupInterface move_group(PLANNING_GROUP_LEFT);

  // get the planning frame
  static const std::string PLANNING_FRAME = move_group.getPlanningFrame(); // cell_base
  ROS_INFO_NAMED("EcoSwash_ROS_cell", "Planning frame: %s", move_group.getPlanningFrame().c_str()); // cell_base
  // ROS_INFO_NAMED("EcoSwash_ROS_cell", "Pose reference frame: %s", move_group.getPoseReferenceFrame().c_str()); // cell_base  

  // robotState object contains all the current pos/vel/acc data.
  moveit::core::RobotStatePtr current_state = move_group.getCurrentState();

  // use raw pointers to refer to the planning group for improved performance
  const robot_state::JointModelGroup* joint_model_group = current_state->getJointModelGroup(PLANNING_GROUP_LEFT);
// TO BE DELETED LATER STOP


  // =============================================================================================================
  // Visualization
  // =============================================================================================================

// TO BE DELETED LATER START
/* namespace rvt = rviz_visual_tools;
  moveit_visual_tools::MoveItVisualTools visual_tools("cell_base");
  robot_l.visual_tools->deleteAllMarkers();

  // remote control is an introspection tool that allows to step through a high level script via buttons in RViz
  robot_l.visual_tools->loadRemoteControl();

  // RViz provides many types of markers
  Eigen::Isometry3d text_pose = Eigen::Isometry3d::Identity();
  text_pose.translation().x() = 0;
  text_pose.translation().y() = 0;
  text_pose.translation().z() = 2.5;
  robot_l.visual_tools->publishText(text_pose, "EcoSwash Cell", rvt::CYAN, rvt::XXLARGE);
  robot_l.visual_tools->trigger();  
*/
// TO BE DELETED LATER STOP

  // =============================================================================================================
  // Getting basic information
  // =============================================================================================================

// TO BE DELETED LATER START
  /* print the name of the active joints
  const std::vector<std::string>& active_joint_names = move_group.getActiveJoints();
  ROS_INFO("Active Joints: \n");
  std::copy(active_joint_names.begin(), active_joint_names.end(), std::ostream_iterator<std::string>(std::cout, ", "));  


  // get a list of all the groups in the robot:
  ROS_INFO_NAMED("EcoSwash_ROS_cellorial", "Available planning groups:");
  std::copy(move_group.getJointModelGroupNames().begin(), 
            move_group.getJointModelGroupNames().end(), std::ostream_iterator<std::string>(std::cout, ", ")); // left_staubli


  // not used for now but can be kept for additional information
  robot_model_loader::RobotModelLoader robot_model_loader("robot_description");
  robot_model::RobotModelPtr kinematic_model = robot_model_loader.getModel();
  ROS_INFO("Model frame: %s \n", kinematic_model->getModelFrame().c_str()); // cell_base 
*/    
// TO BE DELETED LATER STOP


  const std::vector<std::string>& active_joint_names = robot_l.move_group->getActiveJoints(); // defined globally as extern const in moveit_interface.h file

  // =============================================================================================================
  // Reading current pose
  // =============================================================================================================

/* Note about the use of current_state->getGlobalLinkTransform()
  This is somehow the long way of using current pose. I wanted to avoid it since the use of getGlobalLinkTransform() 
  requires a new const declatation every time. Not practical for using the poses each time we'd like to move the robot

  // Read the current pose from the robot model. Isometry3d contains position and orientation in matrix form
  current_state = move_group.getCurrentState();
  const Eigen::Isometry3d& eef_state = current_state->getGlobalLinkTransform(robot_l.eef_link_name.c_str());
  geometry_msgs::Pose pose_eef = tf2::toMsg(eef_state);

  ROS_INFO_STREAM("Start pose of end effector before moving to Home: \n" << pose_eef << "\n");
*/

  // declare a PoseStamped object for the current pose of end effector
  geometry_msgs::PoseStamped pose_stm_eef = move_group.getCurrentPose(robot_l.eef_link_name);

  // print end-effector pose. The pose is in the model frame
  ROS_INFO_STREAM("Pose of end effector before moving to Home: \n" << pose_stm_eef << "\n");

  // declare the ::Plan struct object
  moveit::planning_interface::MoveGroupInterface::Plan my_plan;
  my_plan.planning_time_ = 30;

  // =============================================================================================================
  // Moving the robot to the Home position (plan and move with Joints + ExternalAxis) (joint-space goal)
  // =============================================================================================================

  /* In the emulator, all joint angles start from 0. Thus, the following setting moves the robot to Home. 
     If the robot is already in Home pose for all joints, then the robot does not move */
  double init_posExtAxis = +0.70; // [m]
  double init_rotJ1 = -90.00;  // [deg]
  double init_rotJ2 = +15.00;  // [deg]
  double init_rotJ3 = +135.00; // [deg]
  double init_rotJ4 = 0.00;    // [deg]
  double init_rotJ5 = +30.00;  // [deg]
  double init_rotJ6 = +90.00;  // [deg]

  // Get the current set of joint values for the defined group
  std::vector<double> joint_group_positions;

  // no need to getCurrentState() since no movement was commanded
  current_state->copyJointGroupPositions(joint_model_group, joint_group_positions);

  // log the current joint positions
  log_joint_positions(joint_group_positions);

  // Enter the home values for joints, plan to the new joint space goal and visualize the plan. 
  joint_group_positions[0] = init_posExtAxis;       // [0] has to be external axis due to urdf? tree structure
  joint_group_positions[1] = init_rotJ1 * DEG2RAD;  // deg->radians
  joint_group_positions[2] = init_rotJ2 * DEG2RAD;  
  joint_group_positions[3] = init_rotJ3 * DEG2RAD;  
  joint_group_positions[4] = init_rotJ4 * DEG2RAD; 
  joint_group_positions[5] = init_rotJ5 * DEG2RAD;  
  joint_group_positions[6] = init_rotJ6 * DEG2RAD;  

  // set the entered home joint values in the group
  move_group.setJointValueTarget(joint_group_positions);

  // check if plan is succesful
  success_plan = (move_group.plan(my_plan) == moveit::planning_interface::MoveItErrorCode::SUCCESS);
  sleep(2);
  if(success_plan) { move_group.execute(my_plan); }  

/* Note: Long way of making use of the current state of the end effector. Replaced
  current_state = move_group.getCurrentState();
  const Eigen::Isometry3d& eef_state2 = current_state->getGlobalLinkTransform(robot_l.eef_link_name.c_str());
  geometry_msgs::Pose pose_eef2 = tf2::toMsg(eef_state2);

  ROS_INFO_STREAM("Pose of the end effector after moving to Home: \n" << pose_eef2 << "\n");

*/

  // read the current pose of the end effector from the robot model
  pose_stm_eef = move_group.getCurrentPose(robot_l.eef_link_name);

  // print end-effector pose. The pose is in the model frame
  // Home pose: world_coordinate_x = -5.58; world_coordinate_y = 1.60; world_coordinate_z = 1.64
  ROS_INFO_STREAM("Pose of the end effector after moving to Home: \n" << pose_stm_eef << "\n");

  // =============================================================================================================
  //  JOINT CONSTRAINTS
  // =============================================================================================================  

  /* kinematic_constraints::JointConstraint joint_constraint(kinematic_model);  
  const std::string joint_var_name = joint_constraint.getJointVariableName();
  ROS_INFO_NAMED("EcoSwash_ROS_cell", "name of joint: %s", joint_var_name.c_str());  // empty */

  // get and log the current joint position
  joint_group_positions = move_group.getCurrentJointValues();
  log_joint_positions(joint_group_positions);  

  // constraint for Joint1
  moveit_msgs::JointConstraint joint_constraints;

  // get the name of joint0 (linear axis)
  joint_constraints.joint_name = active_joint_names[0];  // left_external_axis

  // the bound to be achieved is [position - tolerance_below, position + tolerance_above]  
  joint_constraints.position = joint_group_positions[0]; // current pose
 
  j1_tol_below = joint_constraints.position - MIN_J1;
  j1_tol_above = MAX_J1 - joint_constraints.position;

  // Ideally, j1_tol_below should not be <0 when J1 satisfies to stay >700mm on linear axis. But, due to tolerances in execution, 
  // j1_tol_below can be < 0, for which the JointConstraint give a warning and does not function as intended. This is to eliminate the warning
  if(j1_tol_below < 0) {j1_tol_below = 10e-5;} 

  joint_constraints.tolerance_below = j1_tol_below;
  joint_constraints.tolerance_above = j1_tol_above;
  joint_constraints.weight = 1;

  // applying the joint constraints
  moveit_msgs::Constraints test_constraints;
  test_constraints.joint_constraints.push_back(joint_constraints); 
  move_group.setPathConstraints(test_constraints);
  move_group.setPlanningTime(30); // increase planning time due to set joint constraint

  ROS_INFO("Kek4");


  // =============================================================================================================
  //  Planning to the start of Tour
  // =============================================================================================================

  // ---------- Move to the starting pose of the tour ---------- //
  move_rel_pose_x =  1.80; // 1.10 // although constraint_j1 worked, there are poses it cant reach.
  move_rel_pose_y = -0.40;
  move_rel_pose_z = -0.60; // -1.00
  is_relative = 1;

  //set_eef_xyz_pose(move_group, my_plan, move_rel_pose_x, move_rel_pose_y, move_rel_pose_z, robot_l.eef_link_name, is_relative); 
  set_eef_xyz_pose(*robot_l.move_group, robot_l.my_plan2, move_rel_pose_x, move_rel_pose_y, move_rel_pose_z, robot_l.eef_link_name, is_relative); 

  // read the current pose of the end effector from the robot model
  pose_stm_eef = move_group.getCurrentPose(robot_l.eef_link_name);
  robot_l.visual_tools->prompt("Start position (X1Y1Z1) reached. Press 'Next' to orient the end effector"); // robot_l.visual_tools->

  // ====================================================
  // Orienting the toolhead at the current pose
  // ====================================================

  // ---------- Starting orientation config ---------- //
  // define the desired orientation in RPY (Roll wrt world frame X. Pitch wrt wf Y, Yaw wrt Z)
  roll  =   0.0 * DEG2RAD;  // X
  pitch = -90.0 * DEG2RAD;  // Y
  yaw   =   0.0 * DEG2RAD;  // Z
  // set the orientation and rotate the end effector
  rot_c_eef = rotate_eef_through_rpy_set(move_group, my_plan, q_orig, q_des_rot, q_new_rot, roll, pitch, yaw, robot_l.eef_link_name, rot_c_eef);  
  robot_l.visual_tools->prompt("Start orientation reached. Press 'Next' to rotate the end effector");

  // ---------- Rotate the eef towards negative-most yaw value in XY plane ---------- //
  roll  =   0.0 * DEG2RAD;  // X
  pitch =   0.0 * DEG2RAD;  // Y
  for(double yaw_ang_i = step_yaw_deg; yaw_ang_i < max_abs_yaw; yaw_ang_i+=step_yaw_deg)
  {
    yaw =  ROT_CW * step_yaw_deg * DEG2RAD;  // Z 
    rot_c_eef = rotate_eef_through_rpy_set(move_group, my_plan, q_orig, q_des_rot, q_new_rot, roll, pitch, yaw, robot_l.eef_link_name, rot_c_eef);  
		cur_yaw_ang = ROT_CW * yaw_ang_i; // quick and dirty implementation. To be changed later with getRPY
    robot_l.visual_tools->prompt("Pause before next orientation command!");
  }


  // ---------- move towards the center (in Y direction) ---------- //
  abs_posegoal_y = 0.25; 
  move_rel_pose_x = 0;
  move_rel_pose_y = abs_posegoal_y - move_group.getCurrentPose(robot_l.eef_link_name).pose.position.y;
  move_rel_pose_z = 0;
  set_eef_xyz_pose(move_group, my_plan, move_rel_pose_x, move_rel_pose_y, move_rel_pose_z, robot_l.eef_link_name, is_relative); 
  robot_l.visual_tools->prompt("Y2 position (X1Y2Z1) reached. Press 'Next' to rotate the end effector");

  /* Rotate  in +- Yaw first, and then later +- Pitch
  // ---------- Rotate the eef towards positive-most yaw value in XY plane ---------- //
  for(double yaw_ang_i = step_yaw_deg; cur_yaw_ang < max_abs_yaw; cur_yaw_ang+=step_yaw_deg)
  {
    yaw =  ROT_CCW * step_yaw_deg * DEG2RAD;  // Z
		ROS_INFO_STREAM("Current yaw: " << cur_yaw_ang << "[deg]. Step yaw: " << yaw_ang_i << "[deg] \n");
    rot_c_eef = rotate_eef_through_rpy_set(move_group, my_plan, q_orig, q_des_rot, q_new_rot, roll, pitch, yaw, robot_l.eef_link_name, rot_c_eef);  
    robot_l.visual_tools->prompt("Pause before next orientation command!");
  }

  // ---------- Rotate the eef towards negative-most pitch value in XZ plane ---------- //
  roll = 0.0 * DEG2RAD;  // X
  yaw  = 0.0 * DEG2RAD;  // Y
  for(double pitch_ang_i = step_pitch_deg; pitch_ang_i < max_abs_pitch; pitch_ang_i+=step_pitch_deg)
  {
    pitch =  ROT_CW * step_pitch_deg * DEG2RAD;  // Z 
    rot_c_eef = rotate_eef_through_rpy_set(move_group, my_plan, q_orig, q_des_rot, q_new_rot, roll, pitch, yaw, robot_l.eef_link_name, rot_c_eef);  
		cur_pitch_ang = ROT_CW * pitch_ang_i; // quick and dirty implementation. To be changed later with getRPY
    robot_l.visual_tools->prompt("Pause before next orientation command!");
  } */



  // ---------- Rotate the eef towards positive-most (CCW) yaw value in XY plane ---------- //
  roll = 0.0*DEG2RAD;  // X
  for(double yaw_ang_i = cur_yaw_ang; yaw_ang_i < max_abs_yaw; yaw_ang_i+=step_yaw_deg)
  {
    // update the current yaw angle (placed here since the step increment occurs at the end of outer loop)
    cur_pitch_ang = 0*DEG2RAD; // for logging purpose
    cur_yaw_ang = ROT_CCW*yaw_ang_i;

    // Assign the Pitch step after moving with the desired Yaw step
    pitch = ROT_CW*step_pitch_deg*DEG2RAD; // Y
    // do not move the Yaw while stepping through Pitch
    yaw = 0.0*DEG2RAD; // Z

    //  Rotate the eef towards negative-most (CW) Pitch value in XZ plane //
    for(double pitch_ang_i = step_pitch_deg; pitch_ang_i < max_abs_pitch; pitch_ang_i+=step_pitch_deg)
    {
		  ROS_INFO_STREAM("Current yaw  : " << cur_yaw_ang << "[deg]. Current Pitch: " << cur_pitch_ang << "[deg] \n");
      robot_l.visual_tools->prompt("Pause before next Pitch rotation!");
      rot_c_eef = rotate_eef_through_rpy_set(move_group, my_plan, q_orig, q_des_rot, q_new_rot, roll, pitch, yaw, robot_l.eef_link_name, rot_c_eef);  
      cur_pitch_ang = ROT_CW*pitch_ang_i;
    }
    // log the final Pitch value
 	  ROS_INFO_STREAM("Current Yaw: " << cur_yaw_ang << "[deg]. Current pitch: " << cur_pitch_ang << "[deg] \n");
    robot_l.visual_tools->prompt("Press 'Next' to rotate the end effector for next Yaw rotation");

    // Rotate the eef back to natural Pitch = 0 pose (in CCW) and rotate the Yaw by desired step
    pitch = ROT_CW*cur_pitch_ang*DEG2RAD;  // Y
    yaw = ROT_CCW*step_yaw_deg*DEG2RAD; // Z // in the very last step, it will move another step_yaw_deg, which needs to be compansated

    rot_c_eef = rotate_eef_through_rpy_set(move_group, my_plan, q_orig, q_des_rot, q_new_rot, roll, pitch, yaw, robot_l.eef_link_name, rot_c_eef);  
    ROS_INFO("Absolute Pitch=0 and next Yaw step reached.  !");
  }
  robot_l.visual_tools->prompt("All orientations at this pose are done. Press 'Next' to move the end effector to next pose!");
  cur_yaw_ang = cur_yaw_ang + step_yaw_deg; // this is to compansate the last extra yaw rotation at the earlier step

  // ---------- Rotate the eef back to absolute Yaw = 0deg orientation ---------- //
  roll  = 0.0*DEG2RAD; 
  pitch = 0.0*DEG2RAD;
  yaw = ROT_CW*cur_yaw_ang*DEG2RAD;  // Z  
  rot_c_eef = rotate_eef_through_rpy_set(move_group, my_plan, q_orig, q_des_rot, q_new_rot, roll, pitch, yaw, robot_l.eef_link_name, rot_c_eef);  


  // ---------- move up (in +Z direction) ---------- //
  move_rel_pose_x = 0;
  move_rel_pose_y = 0;
  move_rel_pose_z = 0.5;
  set_eef_xyz_pose(move_group, my_plan, move_rel_pose_x, move_rel_pose_y, move_rel_pose_z, robot_l.eef_link_name, is_relative); 
  robot_l.visual_tools->prompt("Z2 position (X1Y2Z2) reached. Press 'Next' to rotate the end effector");


  // ---------- Rotate the eef to negative-most yaw value in XY plane ---------- //
  roll  =   0.0 * DEG2RAD;  // X
  pitch =   0.0 * DEG2RAD;  // Y
  yaw   = -30.0 * DEG2RAD;  // Z
  rot_c_eef = rotate_eef_through_rpy_set(move_group, my_plan, q_orig, q_des_rot, q_new_rot, roll, pitch, yaw, robot_l.eef_link_name, rot_c_eef);  
  robot_l.visual_tools->prompt("Start orientation for X1Y2Z2 reached. Press 'Next' to rotate the end effector");

  cur_yaw_ang = yaw*RAD2DEG;
  // ---------- Rotate the eef towards positive-most (CCW) yaw value in XY plane ---------- //
  roll = 0.0*DEG2RAD;  // X
  for(double yaw_ang_i = cur_yaw_ang; yaw_ang_i < max_abs_yaw; yaw_ang_i+=step_yaw_deg)
  {
    // update the current yaw angle (placed here since the step increment occurs at the end of outer loop)
    cur_pitch_ang = 0*DEG2RAD; // for logging purpose
    cur_yaw_ang = ROT_CCW*yaw_ang_i;

    // Assign the Pitch step after moving with the desired Yaw step
    pitch = ROT_CCW*step_pitch_deg*DEG2RAD; // Y
    // do not move the Yaw while stepping through Pitch
    yaw = 0.0*DEG2RAD; // Z

    //  Rotate the eef towards positive-most (CCW) Pitch value in XZ plane //
    for(double pitch_ang_i = step_pitch_deg; pitch_ang_i < max_abs_pitch; pitch_ang_i+=step_pitch_deg)
    {
		  ROS_INFO_STREAM("Current yaw  : " << cur_yaw_ang << "[deg]. Current Pitch: " << cur_pitch_ang << "[deg] \n");
      robot_l.visual_tools->prompt("Pause before next Pitch rotation!");
      rot_c_eef = rotate_eef_through_rpy_set(move_group, my_plan, q_orig, q_des_rot, q_new_rot, roll, pitch, yaw, robot_l.eef_link_name, rot_c_eef);  
      cur_pitch_ang = ROT_CCW*pitch_ang_i;
    }
    // log the final Pitch value
 	  ROS_INFO_STREAM("Current Yaw: " << cur_yaw_ang << "[deg]. Current pitch: " << cur_pitch_ang << "[deg] \n");
    robot_l.visual_tools->prompt("Press 'Next' to rotate the end effector for next Yaw rotation");

    // Rotate the eef back to natural Pitch = 0 pose (in CCW) and rotate the Yaw by desired step
    pitch = ROT_CW*cur_pitch_ang*DEG2RAD;  // Y
    yaw = ROT_CCW*step_yaw_deg*DEG2RAD; // Z

    rot_c_eef = rotate_eef_through_rpy_set(move_group, my_plan, q_orig, q_des_rot, q_new_rot, roll, pitch, yaw, robot_l.eef_link_name, rot_c_eef);  
    ROS_INFO("Absolute Pitch=0 and next Yaw step reached.  !");
  }
  robot_l.visual_tools->prompt("All orientations at this pose are done. Press 'Next' to move the end effector to next pose!");
  cur_yaw_ang = cur_yaw_ang + step_yaw_deg; // this is to compansate the last extra yaw rotation at the earlier step


  // ---------- Rotate the eef back to absolute Yaw = 0deg orientation ---------- //
  roll  =  0.0 * DEG2RAD; 
  pitch =  0.0 * DEG2RAD;
  yaw = ROT_CW * cur_yaw_ang * DEG2RAD;  // Z  
  rot_c_eef = rotate_eef_through_rpy_set(move_group, my_plan, q_orig, q_des_rot, q_new_rot, roll, pitch, yaw, robot_l.eef_link_name, rot_c_eef); 
















  // =============================================================================================================
  // Planning to a pose goal
  // =============================================================================================================

/* PLANNING TO A POSE GOAL WITH COMPUTE CARTESIAN PATH 

  // read the current pose of the end effector from the robot model
  pose_stm_eef = move_group.getCurrentPose(robot_l.eef_link_name);

  // print end-effector pose. The pose is in the model frame
  ROS_INFO_STREAM("Start pose of end effector after moving: \n" << pose_stm_eef << "\n");

  // computeCartesianPath() only accepts geometry_msgs::Pose type object
  std::vector<geometry_msgs::Pose> waypoints;

  /* Note!! If the current pose added as the first waypoint, skip this point in the trajectory.
  //          Otherwise, time at t[0]=t[1]=0 and robot does not move thinking that it is already in the goal state!
  
  // add the  current pose into the Pose messages vector (for the display)
  waypoints.push_back(pose_stm_eef.pose); 

  geometry_msgs::Pose target_poses1 = pose_stm_eef.pose; // define target with a temporary assignment of start_pose
  geometry_msgs::Pose end_pose1 = pose_stm_eef.pose;     // define the end_pose same as start_pose to bring the robot back to start
 
  // random assignment of displacements in x, y, z for meander movement like path planning trial
  double init_shift_x = 2.0;  // forward - backward (assuming the start pose is near Home position, just a dirty implementation for now)
  double shift_x = 3.0;       // forward - backward
  double shift_y = 0.3;       // left - right
  double shift_z = 0.25;      // up - down
  double disturb_move = 0.05; // random disturbance value

  target_poses1.position.x += init_shift_x - disturb_move;
  target_poses1.position.y -= shift_y;
  target_poses1.position.z -= shift_z - disturb_move;
  waypoints.push_back(target_poses1);  // initial move to start a randomly selected pose

  target_poses1.position.x += shift_x;
  waypoints.push_back(target_poses1);  // forward

  target_poses1.position.z -= shift_z;
  waypoints.push_back(target_poses1);  // down

  target_poses1.position.x -= shift_x;
  waypoints.push_back(target_poses1);  // backward

  target_poses1.position.z -= shift_z;
  waypoints.push_back(target_poses1);  // down

  target_poses1.position.x += shift_x;
  waypoints.push_back(target_poses1);  // forward

  // Visualize the plan in RViz
  robot_l.visual_tools->deleteAllMarkers();
  robot_l.visual_tools->publishText(text_pose, "End effector goal", rvt::WHITE, rvt::XXLARGE);
  robot_l.visual_tools->publishPath(waypoints, rvt::LIME_GREEN, rvt::SMALL);

  // visualize the manually added waypoint poses
  bool b_visualize_waypoints_flag = true;
  if (b_visualize_waypoints_flag)
  {
    for (std::size_t i = 0; i < waypoints.size(); ++i) {
      robot_l.visual_tools->publishAxisLabeled(waypoints[i], "pt" + std::to_string(i), rvt::SMALL);
    }  
  }
  ROS_INFO_NAMED("EcoSwash_ROS_cell", "Size of waypoints: %zu", waypoints.size());
  robot_l.visual_tools->trigger();



  // =============================================================================================================
  // Planning and execution of a manually designed meander movement (through computeCartesianPath)
  // =============================================================================================================

  // If the start pose is the same as current pose, the robot doesn't move because trajectory[1].time = 0
  //   This is an issue for the move/execute methods. So, initially disturb the tool pose and skip the start pose in the waypoints vector
  std::vector<geometry_msgs::Pose> sub_waypoints(&waypoints[1], &waypoints[waypoints.size()]);; // if waypoints[0] = start pose
    
  moveit_msgs::RobotTrajectory trajectory;
  const double jump_threshold = 5.0; // deafult = 0
  const double eef_step = 0.1;    // 0.01
  bool avoid_collision = true;

  double fraction_success = 0.0;
  int attempt_count = 0;
  int max_attempt = 10; // If IKFast cannot find it, no need to loop 1000 times.
  while(fraction_success != 1.0 && attempt_count <= max_attempt)
  {
    // fraction_success = move_group.computeCartesianPath(waypoints, eef_step, jump_threshold, trajectory, avoid_collision); // if waypoints[0] = next pose
    fraction_success = move_group.computeCartesianPath(sub_waypoints, eef_step, jump_threshold, trajectory, avoid_collision); // if waypoints[0] = next pose
    attempt_count++;
    ROS_INFO("Planning attempt: %d", attempt_count);
  }

  attempt_count--;
  ROS_INFO_NAMED("EcoSwash_ROS_cell", "Visualizing plan (cartesian path) after %d count (%.2f%% acheived)", attempt_count, fraction_success * 100.0);

  // Creare a vector array for trajectory object
  std::vector<double> pt_time_v(trajectory.joint_trajectory.points.size(), -1);
 
  // check the time start for each point in the trajectory
  for(std::size_t ind=0; ind < trajectory.joint_trajectory.points.size(); ++ind) {
    pt_time_v[ind] = trajectory.joint_trajectory.points[ind].time_from_start.toSec();
  }
  // Log the time info
  ROS_INFO_NAMED("EcoSwash_ROS_cell", "Size of joint_trajectory.points: %zu", pt_time_v.size());
  ROS_INFO("trajectory.joint_trajectory.points[ ].time_from_start.toSec() values: ");
  std::copy(pt_time_v.begin(), pt_time_v.end(), std::ostream_iterator<double>(std::cout, ", "));   
  
  robot_l.visual_tools->prompt("Pause until 'Next' is pressed in Rviz (to actually move the robot along the trajectory)");

  // => Execute for long the cartesian trajectory with small eef mostly not successful
  my_plan.trajectory_ = trajectory;
  sleep(2.0);

  ROS_INFO("Execution LOG-1");
  move_group.execute(my_plan);
  ROS_INFO("Execution LOG-2");

  // Move the end effector to home position
  robot_l.visual_tools->prompt("Pause until 'Next' is pressed in Rviz - Move the robot to home position");

  // Clear the pose targets
  move_group.clearPoseTarget();
  // move_group.setStartStateToCurrentState();

  // read the current pose of the end effector from the robot model
  pose_stm_eef = move_group.getCurrentPose(robot_l.eef_link_name);
  ROS_INFO_STREAM("Pose of end effector after pose target clearance: \n" << pose_stm_eef << "\n");

  // Set the pose as the Start of the Tour
  pose_stm_eef.pose.position.z += 0.2; 
  move_group.setPoseTarget(pose_stm_eef);

  success_plan = (move_group.plan(my_plan) == moveit::planning_interface::MoveItErrorCode::SUCCESS);
  sleep(2);
  if(success_plan) { move_group.execute(my_plan); }  

  // read the current pose of the end effector from the robot model
  pose_stm_eef = move_group.getCurrentPose(robot_l.eef_link_name);
  ROS_INFO_STREAM("Pose of end effector after setting a near target: \n" << pose_stm_eef << "\n"); 

  // Move the base back to home position
  joint_group_positions[0] = init_posExtAxis;
  move_group.setJointValueTarget(joint_group_positions);

  success_plan = (move_group.plan(my_plan) == moveit::planning_interface::MoveItErrorCode::SUCCESS);
  sleep(2);
  if(success_plan) { move_group.execute(my_plan); }


*/

  // =============================================================================================================
  // End of the program
  // =============================================================================================================
  ros::shutdown();
  return 0;
}
