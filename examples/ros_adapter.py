"""Copyright 2012, System Insights, Inc.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License."""
    
import sys, os, time

path, file = os.path.split(__file__)
sys.path.append(os.path.realpath(path) + '/../src')

from data_item import Event, SimpleCondition, Sample, ThreeDSample
from mtconnect_adapter import Adapter

import roslib
roslib.load_manifest('turtlesim')
import rospy
import turtlesim.msg

adapter = Adapter(('0.0.0.0', 7878))
pose = ThreeDSample('pose')
adapter.add_data_item(pose)

def handle_turtle_pose(msg, turtlename):
    adapter.begin_gather()
    pose.set_value((msg.x, msg.y, 0.0))
    adapter.complete_gather()

if __name__ == "__main__":
    avail = Event('avail')
    adapter.add_data_item(avail)
    avail.set_value('AVAILABLE')
    adapter.start()
    rospy.init_node('mtconnect_adapter')
    rospy.Subscriber('/turtle1/pose',
                     turtlesim.msg.Pose,
                     handle_turtle_pose,
                     'turtle1')
    rospy.spin()

    
