/*
 * move_arm_client_node.cpp
 *
 *  Created on: Jan 16, 2013
 */
#include <ros/ros.h>
#include <arm_navigation_msgs/GetPlanningScene.h>
#include <arm_navigation_msgs/SetPlanningSceneDiff.h>
#include <planning_environment/models/collision_models.h>
#include <planning_environment/models/model_utils.h>
#include <actionlib/client/simple_action_client.h>
#include <arm_navigation_msgs/MoveArmAction.h>
#include <arm_navigation_msgs/utils.h>
#include <nav_msgs/Path.h>
#include <tf/tf.h>
#include <tf/transform_listener.h>
#include <geometry_msgs/PoseArray.h>
#include <boost/tuple/tuple.hpp>

// aliases
typedef actionlib::SimpleActionClient<arm_navigation_msgs::MoveArmAction> MoveArmClient;
typedef boost::shared_ptr<MoveArmClient> MoveArmClientPtr;
typedef boost::shared_ptr<planning_environment::CollisionModels> CollisionModelsPtr;
typedef boost::shared_ptr<planning_models::KinematicState> KinematicStatePtr;
typedef boost::tuple<std::string,std::string,tf::Transform> CartesianGoal;

// defaults
static const std::string DEFAULT_PATH_MSG_TOPIC = "move_arm_path";
static const std::string DEFAULT_MOVE_ARM_ACTION = "move_arm_action";
static const std::string DEFAULT_PATH_PLANNER = "/ompl_planning/plan_kinematic_path";
static const std::string DEFAULT_PLANNING_SCENE_DIFF_SERVICE = "/environment_server/set_planning_scene_diff";
static const int DEFAULT_PATH_PLANNING_ATTEMPTS = 1;
static const double DEFAULT_PATH_PLANNING_TIME = 5.0f;

// ros parameters
static const std::string PARAM_ARM_GROUP = "arm_group";

class MoveArmActionClient
{

public:
	struct CartesianTrajectory
	{
	public:
		CartesianTrajectory()
		{

		}

		~CartesianTrajectory()
		{

		}

		std::string toString()
		{
			std::stringstream ss;
			ss<<"\n";
			ss<<"arm_group: "<<arm_group_<<"\n";
			ss<<"frame_id: "<<frame_id_<<"\n";
			ss<<"tool_name: "<<link_name_<<"\n";
			ss<<"via points: "<<"transform array with "<<cartesian_points_.size()<<" elements"<<"\n";
			tf::Vector3 pos;
			tf::Quaternion q;
			for(std::size_t i = 0; i < cartesian_points_.size(); i++)
			{
				tf::Transform &t = cartesian_points_[i];
				pos = t.getOrigin();
				q = t.getRotation();

				ss<<"\t - pos:[ "<<pos.getX()<<", "<<pos.getY()<<", "<<pos.getZ()<<"]\n";
				ss<<"\t   axis:[ "<<q.getAxis().getX()<<", "<<q.getAxis().getY()<<", "<<
						q.getAxis().getZ()<<"], angle: "<< q.getAngle()<<"\n";
			}

			return ss.str();
		}

		bool parseParam(XmlRpc::XmlRpcValue &paramVal)
		{
			XmlRpc::XmlRpcValue cartesian_points_param;
			ros::NodeHandle nh;

			bool success = true;
			if(paramVal.getType() == XmlRpc::XmlRpcValue::TypeStruct)
			{
				// fetching frame id and link name
				arm_group_ = static_cast<std::string>(paramVal["arm_group"]);
				frame_id_ = static_cast<std::string>(paramVal["frame_id"]);
				link_name_ = static_cast<std::string>(paramVal["tool_name"]);

				// fetching cartesian via point struct array
				cartesian_points_param = paramVal["via_points"];

				// parsing each struct
				if((cartesian_points_param.getType() == XmlRpc::XmlRpcValue::TypeArray)
						&& (cartesian_points_param[0].getType() == XmlRpc::XmlRpcValue::TypeStruct))
				{
					tf::Vector3 pos, axis;
					tf::Quaternion q;
					double val;
					XmlRpc::XmlRpcValue structElmt, posElmt, rotElmt, axisElmt;
					cartesian_points_.clear();

					ROS_INFO_STREAM(ros::this_node::getName()<<": parsing via points struct array"<<toString());
					for(std::size_t i = 0; i < cartesian_points_param.size(); i++)
					{
						structElmt = cartesian_points_param[i];
						posElmt = structElmt["position"];
						rotElmt = structElmt["orientation"];
						axisElmt = rotElmt["axis"];

						// fetching position
						val = static_cast<double>(posElmt["x"]);pos.setX(val);
						val = static_cast<double>(posElmt["y"]);pos.setY(val);
						val = static_cast<double>(posElmt["z"]);pos.setZ(val);

						// fetching rotation
						val = static_cast<double>(axisElmt["x"]);axis.setX(val);
						val = static_cast<double>(axisElmt["y"]);axis.setY(val);
						val = static_cast<double>(axisElmt["z"]);axis.setZ(val);
						val = static_cast<double>(rotElmt["angle"]);
						q.setRotation(axis,val);

						// storing as transform
						cartesian_points_.push_back(tf::Transform(q,pos));
					}
				}
				else
				{
					ROS_ERROR_STREAM(ros::this_node::getName()<<": Parsing error in cartesian_trajectory parameter");
					success = false;
				}
			}
			else
			{
				ROS_ERROR_STREAM(ros::this_node::getName()<<": Parsing error in cartesian_trajectory parameter");
				success = false;
			}

			ROS_INFO_STREAM(ros::this_node::getName()<<": cartesian_trajectory parameter successfully parsed"<<toString());
			return success;
		}

		void getMarker(nav_msgs::Path &p)
		{
			p.header.frame_id = frame_id_;
			p.header.stamp = ros::Time(0.0f);
			geometry_msgs::PoseStamped poseSt;

			// initializing pose stamped object
			poseSt.header.frame_id = frame_id_;
			poseSt.header.stamp = ros::Time(0.0f);

			std::vector<tf::Transform>::iterator i;
			for(i = cartesian_points_.begin(); i != cartesian_points_.end(); i++)
			{
				tf::poseTFToMsg(*i,poseSt.pose);
				p.poses.push_back(poseSt);
			}

		}

		bool fetchParameters(std::string nameSpace= "")
		{
			XmlRpc::XmlRpcValue val;
			ros::NodeHandle nh;
			bool success = nh.getParam(nameSpace + "/cartesian_trajectory",val) && parseParam(val);
			if(!success)
			{
				ROS_ERROR_STREAM(ros::this_node::getName()<<": Parsing error in cartesian_trajectory parameter");
			}
			return success;
		}

		std::string arm_group_;
		std::string frame_id_;
		std::string link_name_;
		std::vector<tf::Transform> cartesian_points_;
	};
public:
	MoveArmActionClient()
	:
		move_arm_client_ptr_()
	{

	}

	~MoveArmActionClient()
	{

	}

	void run()
	{
		using namespace arm_navigation_msgs;

		if(!setup())
		{
			return;
		}

		ros::AsyncSpinner spinner(2);
		spinner.start();

		// producing cartesian goals in term of arm base and tip link
		geometry_msgs::PoseArray cartesian_poses;
		if(!getTrajectoryInArmSpace(cartesian_traj_,cartesian_poses))
		{
			ROS_ERROR_STREAM(ros::this_node::getName()<<": Cartesian Trajectory could not be transformed to arm space");
			return;
		}

		while(ros::ok())
		{
			// proceeding if latest robot state can be retrieved
//			if(!getArmStartState(cartesian_traj_.arm_group_,goal.motion_plan_request.start_state))
//			{
//				continue;
//			}

			if(!moveArm(cartesian_poses))
			{
				ros::Duration(4.0f).sleep();
			}
		}
	}

	/*
	 * Takes and array of poses where each pose is the desired tip link pose described in terms of the arm base
	 */
	bool moveArm(const geometry_msgs::PoseArray &cartesian_poses)
	{
		using namespace arm_navigation_msgs;

		ROS_INFO_STREAM(ros::this_node::getName()<<": Sending Cartesian Goal with "<<cartesian_poses.poses.size()<<" via points");
		bool success = !cartesian_poses.poses.empty();
		std::vector<geometry_msgs::Pose>::const_iterator i;
		for(i = cartesian_poses.poses.begin(); i != cartesian_poses.poses.end();i++)
		{
			// clearing goal
			move_arm_goal_.motion_plan_request.goal_constraints.position_constraints.clear();
			move_arm_goal_.motion_plan_request.goal_constraints.orientation_constraints.clear();
			move_arm_goal_.motion_plan_request.goal_constraints.joint_constraints.clear();
			move_arm_goal_.motion_plan_request.goal_constraints.visibility_constraints.clear();

			//tf::poseTFToMsg(*i,move_pose_constraint_.pose);
			move_pose_constraint_.pose = *i;
			arm_navigation_msgs::addGoalConstraintToMoveArmGoal(move_pose_constraint_,move_arm_goal_);

			// sending goal
			move_arm_client_ptr_->sendGoal(move_arm_goal_);
			success = move_arm_client_ptr_->waitForResult(ros::Duration(200.0f));
			if(success)
			{
				success = actionlib::SimpleClientGoalState::SUCCEEDED == move_arm_client_ptr_->getState().state_;
				if(success)
				{
					ROS_INFO_STREAM(ros::this_node::getName()<<": Goal Achieved");
				}
				else
				{
					ROS_ERROR_STREAM(ros::this_node::getName()<<": Goal Rejected with error flag: "
							<<(unsigned int)move_arm_client_ptr_->getState().state_);
					break;
				}

			}
			else
			{
				move_arm_client_ptr_->cancelGoal();
				ROS_ERROR_STREAM(ros::this_node::getName()<<": Goal Failed with error flag: "
						<<(unsigned int)move_arm_client_ptr_->getState().state_);
				break;
			}
		}

		return success;
	}

	bool fetchParameters(std::string nameSpace = "")
	{
		ros::NodeHandle nh("~");

		bool success =  nh.getParam(PARAM_ARM_GROUP,arm_group_) && cartesian_traj_.fetchParameters(nameSpace);
		return success;
	}

	void timerCallback(const ros::TimerEvent &evnt)
	{
		path_pub_.publish(path_msg_);
	}

protected:

	bool getArmStartState(std::string group_name, arm_navigation_msgs::RobotState &robot_state)
	{
		using namespace arm_navigation_msgs;

		// calling set planning scene service
		SetPlanningSceneDiff::Request planning_scene_req;
		SetPlanningSceneDiff::Response planning_scene_res;
		if(planning_scene_client_.call(planning_scene_req,planning_scene_res))
		{
			ROS_INFO_STREAM(ros::this_node::getName()<<": call to set planning scene succeeded");
		}
		else
		{
			ROS_ERROR_STREAM(ros::this_node::getName()<<": call to set planning scene failed");
			return false;
		}

		// updating robot kinematic state from planning scene
		planning_models::KinematicState *st = collision_models_ptr_->setPlanningScene(planning_scene_res.planning_scene);
		if(st == NULL)
		{
			ROS_ERROR_STREAM(ros::this_node::getName()<<": Kinematic State for arm could not be retrieved from planning scene");
			return false;
		}

		planning_environment::convertKinematicStateToRobotState(*st,
															  ros::Time::now(),
															  collision_models_ptr_->getWorldFrameId(),
															  robot_state);

		collision_models_ptr_->revertPlanningScene(st);
		return true;
	}

	bool setup()
	{
		ros::NodeHandle nh;
		bool success = true;

		if(!fetchParameters())
		{
			return false;
		}

		// setting up action client
		move_arm_client_ptr_ = MoveArmClientPtr(new MoveArmClient(DEFAULT_MOVE_ARM_ACTION,true));
		unsigned int attempts = 0;
		while(attempts++ < 20)
		{
			ROS_WARN_STREAM(ros::this_node::getName()<<": waiting for "<<DEFAULT_MOVE_ARM_ACTION<<" server");
			success = move_arm_client_ptr_->waitForServer(ros::Duration(5.0f));
			if(success)
			{
				ROS_INFO_STREAM(ros::this_node::getName()<<": Found "<<DEFAULT_MOVE_ARM_ACTION<<" server");
				break;
			}
		}

		// setting up service clients
		planning_scene_client_ = nh.serviceClient<arm_navigation_msgs::SetPlanningSceneDiff>(DEFAULT_PLANNING_SCENE_DIFF_SERVICE);

		// setting up ros publishers
		path_pub_ = nh.advertise<nav_msgs::Path>(DEFAULT_PATH_MSG_TOPIC,1);
		cartesian_traj_.getMarker(path_msg_);

		// setting up ros timers
		publish_timer_ = nh.createTimer(ros::Duration(2.0f),&MoveArmActionClient::timerCallback,this);

		/*
		 * TODO
		 * This node will need to have the capability of taking a cartesian trajectory
		 * defined in an arbitrary coordinate frame and perform the corresponding
		 * transformations before it sends it to the move arm action server
		 */

		// obtaining arm info
		collision_models_ptr_ = CollisionModelsPtr(new planning_environment::CollisionModels("robot_description"));
		const planning_models::KinematicModel::JointModelGroup *joint_model_group =
						collision_models_ptr_->getKinematicModel()->getModelGroup(arm_group_);
		const std::vector<const planning_models::KinematicModel::JointModel *> &joint_models_ = joint_model_group->getJointModels();

		// finding arm base and tip links
		base_link_frame_id_ = joint_models_.front()->getParentLinkModel()->getName();
		tip_link_frame_id_ = joint_models_.back()->getChildLinkModel()->getName();

		// initializing move arm request members
		move_arm_goal_.motion_plan_request.group_name = arm_group_;
		move_arm_goal_.motion_plan_request.num_planning_attempts = DEFAULT_PATH_PLANNING_ATTEMPTS;
		move_arm_goal_.planner_service_name = DEFAULT_PATH_PLANNER;
		move_arm_goal_.motion_plan_request.planner_id = "";
		move_arm_goal_.motion_plan_request.allowed_planning_time = ros::Duration(DEFAULT_PATH_PLANNING_TIME);

		move_pose_constraint_.header.frame_id = base_link_frame_id_;
		move_pose_constraint_.link_name = tip_link_frame_id_;
		move_pose_constraint_.absolute_position_tolerance.x = 0.02f;
		move_pose_constraint_.absolute_position_tolerance.y = 0.02f;
		move_pose_constraint_.absolute_position_tolerance.z = 0.02f;
		move_pose_constraint_.absolute_roll_tolerance = 0.04f;
		move_pose_constraint_.absolute_pitch_tolerance = 0.04f;
		move_pose_constraint_.absolute_yaw_tolerance = 0.04f;

		// printing arm details
		const std::vector<std::string> &joint_names = joint_model_group->getJointModelNames();
		const std::vector<std::string> &link_names = joint_model_group->getGroupLinkNames();
		std::vector<std::string>::const_iterator i;
		std::stringstream ss;
		ss<<"\nArm Base Link Frame: "<<base_link_frame_id_;
		ss<<"\nArm Tip Link Frame: "<<tip_link_frame_id_;
		ss<<"\nJoint Names in group '"<<arm_group_<<"'";
		for(i = joint_names.begin(); i != joint_names.end() ; i++)
		{
			ss<<"\n\t"<<*i;
		}

		ss<<"\nLink Names in group '"<<arm_group_<<"'";
		for(i = link_names.begin(); i != link_names.end() ; i++)
		{
			ss<<"\n\t"<<*i;
		}
		ROS_INFO_STREAM(ros::this_node::getName()<<ss.str());

		return success;
	}

	bool getPoseInArmSpace(const CartesianGoal &cartesian_goal,geometry_msgs::Pose &base_to_tip_pose)
	{
		// declaring transforms
		static tf::StampedTransform base_to_parent_tf;
		static tf::StampedTransform tcp_to_tip_link_tf;
		tf::Transform base_to_tip_tf; // desired pose of tip link in base coordinates

		const std::string &parent_frame = boost::tuples::get<0>(cartesian_goal);
		const std::string &tcp_frame = boost::tuples::get<1>(cartesian_goal);
		const tf::Transform &parent_to_tcp_tf = boost::tuples::get<2>(cartesian_goal);

		// finding transforms
		try
		{
			tf_listener_.lookupTransform(parent_frame,base_link_frame_id_,ros::Time(0),base_to_parent_tf);
			tf_listener_.lookupTransform(tip_link_frame_id_,tcp_frame,ros::Time(0),tcp_to_tip_link_tf);
		}
		catch(tf::LookupException &e)
		{
			ROS_ERROR_STREAM(ros::this_node::getName()<<": Unable to lookup transforms");
			return false;
		}

		base_to_tip_tf = ((tf::Transform)base_to_parent_tf) * parent_to_tcp_tf * ((tf::Transform)tcp_to_tip_link_tf);
		tf::poseTFToMsg(base_to_tip_tf,base_to_tip_pose);

		return true;
	}

	bool getTrajectoryInArmSpace(const CartesianTrajectory &cartesian_traj,geometry_msgs::PoseArray &base_to_tip_poses)
	{
		// declaring transforms
		static tf::StampedTransform base_to_parent_tf;
		static tf::StampedTransform tcp_to_tip_link_tf;
		tf::Transform base_to_tip_tf; // desired pose of tip link in base coordinates
		geometry_msgs::Pose base_to_tip_pose;

		// finding transforms
		try
		{
			tf_listener_.lookupTransform(cartesian_traj.frame_id_,base_link_frame_id_,ros::Time(0),base_to_parent_tf);
			tf_listener_.lookupTransform(tip_link_frame_id_,cartesian_traj.link_name_,ros::Time(0),tcp_to_tip_link_tf);
		}
		catch(tf::LookupException &e)
		{
			ROS_ERROR_STREAM(ros::this_node::getName()<<": Unable to lookup transforms");
			return false;
		}

		std::vector<tf::Transform>::const_iterator i;
		const std::vector<tf::Transform> &goal_array = cartesian_traj.cartesian_points_;
		for(i = goal_array.begin(); i != goal_array.end(); i++)
		{
			base_to_tip_tf = (static_cast<tf::Transform>(base_to_parent_tf)) * (*i) * (static_cast<tf::Transform>(tcp_to_tip_link_tf));
			tf::poseTFToMsg(base_to_tip_tf,base_to_tip_pose);
			base_to_tip_poses.poses.push_back(base_to_tip_pose);
		}

		return true;
	}

protected:

	// ros action clients
	MoveArmClientPtr move_arm_client_ptr_;

	// ros service clients
	ros::ServiceClient planning_scene_client_;

	// ros publishers
	ros::Publisher path_pub_;

	// ros timers
	ros::Timer publish_timer_;

	// ros messages
	nav_msgs::Path path_msg_;

	// tf listener
	tf::TransformListener tf_listener_;

	// arm info
	CollisionModelsPtr collision_models_ptr_;
	KinematicStatePtr arm_kinematic_state_ptr_;
	std::string base_link_frame_id_;
	std::string tip_link_frame_id_;

	// ros parameters
	CartesianTrajectory cartesian_traj_;
	std::string arm_group_;

	// move arm request members
	arm_navigation_msgs::MoveArmGoal move_arm_goal_;
	arm_navigation_msgs::SimplePoseConstraint move_pose_constraint_;

};

int main(int argc,char** argv)
{
	ros::init(argc,argv,"move_arm_client_node");
	ros::NodeHandle nh;

	MoveArmActionClient client;
	client.run();

	return 0;
}
