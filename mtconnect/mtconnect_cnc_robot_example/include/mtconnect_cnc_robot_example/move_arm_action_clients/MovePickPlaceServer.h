/*
 * MovePickPlaceServer.h
 *
 *  Created on: Jan 25, 2013
 *      Author: coky
 */

#ifndef MOVEPICKPLACESERVER_H_
#define MOVEPICKPLACESERVER_H_

#include "MoveArmActionClient.h"
#include <object_manipulator/grasp_execution/approach_lift_grasp.h>
#include <object_manipulator/place_execution/descend_retreat_place.h>
#include <object_manipulation_msgs/PickupAction.h>
#include <object_manipulation_msgs/PlaceAction.h>
#include <object_manipulation_msgs/GraspHandPostureExecutionAction.h>
#include <actionlib/server/simple_action_server.h>

// aliases
typedef actionlib::ActionServer<object_manipulation_msgs::PickupAction> MoveArmPickupServer;
typedef actionlib::ActionServer<object_manipulation_msgs::PlaceAction> MoveArmPlaceServer;
typedef actionlib::SimpleActionClient<object_manipulation_msgs::GraspHandPostureExecutionAction> GraspActionClient;
typedef boost::shared_ptr<MoveArmPickupServer> MoveArmPickupServerPtr;
typedef boost::shared_ptr<MoveArmPlaceServer> MoveArmPlaceServerPtr;
typedef boost::shared_ptr<GraspActionClient> GraspActionClientPtr;
typedef MoveArmPickupServer::GoalHandle PickupGoalHandle;
typedef MoveArmPlaceServer::GoalHandle PlaceGoalHandle;

// defaults
static const std::string DEFAULT_PICKUP_ACTION = "pickup_action_service";
static const std::string DEFAULT_PLACE_ACTION = "place_action_service";
static const std::string DEFAULT_GRASP_ACTION = "grasp_action_service";

class MovePickPlaceServer: public MoveArmActionClient
{
public:
	MovePickPlaceServer();
	virtual ~MovePickPlaceServer();

	virtual void run();
	virtual bool fetchParameters(std::string nameSpace = "");

	virtual bool moveArmThroughPickSequence(object_manipulation_msgs::PickupGoal &pickup_goal);
	virtual bool moveArmThroughPlaceSequence(object_manipulation_msgs::PlaceGoal &place_goal);

protected:

	virtual bool setup();

protected:

	virtual void pickupGoalCallback(PickupGoalHandle goal);
	virtual void pickupCancelCallback(PickupGoalHandle goal);
	virtual void placeGoalCallback(PlaceGoalHandle goal);
	virtual void placeCancelCallback(PlaceGoalHandle goal);

	void createPickupMoveSequence(const object_manipulation_msgs::PickupGoal &goal,object_manipulator::GraspExecutionInfo &seq);
	void createPlaceMoveSequence(const object_manipulation_msgs::PlaceGoal &goal,object_manipulator::PlaceExecutionInfo &seq);

protected:

	// action servers
	MoveArmPickupServerPtr arm_pickup_server_ptr_;
	MoveArmPlaceServerPtr arm_place_server_ptr_;

	// action clients
	GraspActionClientPtr grasp_action_client_ptr_;

};

#endif /* MOVEPICKPLACESERVER_H_ */
