#! /usr/bin/env python

"""
   Copyright 2013 Southwest Research Institute
 
   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at
 
     http://www.apache.org/licenses/LICENSE-2.0
 
   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
   """

# Import standard Python modules
import sys
import os
import operator
import thread
import re
import time
from importlib import import_module
from httplib import HTTPConnection
from xml.etree import ElementTree

# Import custom Python module to read config file
import read_config_file

# Import custom Python modules for MTConnect Adapter interface
path, file = os.path.split(__file__)
sys.path.append(os.path.realpath(path) + '/src')
from data_item import Event, SimpleCondition, Sample, ThreeDSample
from mtconnect_adapter import Adapter
from long_pull import LongPull

# Import ROS Python modules
import roslib
import rospy
import actionlib

class GenericActionClient():
    def __init__(self):
        # Initialize ROS generic client node
        rospy.init_node('ActionClient')
        
        # Setup MTConnect to ROS Conversion
        self.config = read_config_file.obtain_dataMap()
        
        # Setup MTConnect Adapter for robot status data items
        self.adapter = Adapter(('0.0.0.0', self.config['Port']))
        self.ns = dict(m = 'urn:mtconnect.org:MTConnectStreams:1.2')
        
        # Create empty lists for actions, message type handles, etc.
        self.lib_manifests = []
        self.type_handle = None
        self.action_list = {}
        self.action_goals = {}            
        self.action_conv = []
        self.namespace = None
        self.xml_goal = None
        self.di_dict = {} # XML data item dictionary to store MTConnect Adapter Events
        self.handshake = None
        
        # Setup action client data per the config file
        self.setup_topic_data()
        
        # Add data items to the MTConnect Adapter - must be unique data items
        self.add_agent()
        
        # Start the adapter
        rospy.loginfo('Start the Robot Link adapter')
        self.adapter.start()
        
        # Establish XML connection, read in current XML
        self.conn = HTTPConnection('localhost', self.config['localhost'])
        response = self.xml_get_response("/cnc/current")
        cnc_body = response.read()
        
        # Parse the XML and determine the current sequence and XML Event elements
        seq, elements = self.xml_components(cnc_body)
        
        # Start a streaming XML connection
        response = self.xml_get_response("/cnc/sample?interval=1000&count=1000&from=" + seq)
        
        # Create class lock
        self.lock = thread.allocate_lock()

        # Create XML polling thread
        lp = LongPull(response)
        lp.long_pull(self.xml_callback) # Runs until user interrupts
    
    def add_agent(self):
        for xml_tag in self.action_list:
            data_item = self.split_event(xml_tag)
            self.di_dict[data_item] = Event(data_item)
            self.adapter.add_data_item(self.di_dict[data_item])
            
            rospy.loginfo('data_item --> %s' % xml_tag)
        
        # Set initial states for robot actions
        self.adapter.begin_gather()
        for data_item, event in self.di_dict.items():
            event.set_value('READY')
        self.adapter.complete_gather()
        return
    
    def split_event(self, xml_tag):
        tokens = re.findall(r'([A-Z][a-z]*)', xml_tag)
        tokenlist = [val.lower() for val in tokens]
        data_item = tokenlist[0] + '_' + tokenlist[1]
        return data_item
                
    def setup_topic_data(self):
        """This function captures the topic namespace, type, action goals,
        and action state conversion from ROS to MTConnect as required for
        the ROS action client.  This task completed for each topic specified
        in the configuration file.
        
        This function then performs a relative import of the topic
        via the getattr(import_module) function.  Data is stored in the
        following class attributes:
        
            self.lib_manifests --> used to track which manifests have been loaded
            self.type_handle   --> used for ROS SimpleActionClient, stores namespace with action messages
            self.action_list   --> used for ROS SimpleActionClient, stores CNC action request strings
            self.action_goals  --> data structure for message class instances of the topic type
        """
        
        for namespace, action in self.config.items():
            if namespace != 'localhost' and namespace != 'Port': # Only one ROS namespace in config by design
                # Load package manifest if unique
                if namespace not in self.lib_manifests:
                    roslib.load_manifest(namespace)
                    self.lib_manifests.append(namespace)

                # Import module
                rospy.loginfo('Importing --> ' + namespace + '.msg')
                self.type_handle = import_module(namespace + '.msg')
                
                # Capture namespace for action client
                self.namespace = namespace
                
                for action_req in action.keys():
                    # Capture action name for action client callback reference
                    self.action_list[action_req] = action_req
                    
                    # Capture ROS Action parameters
                    goal_key = self.config[namespace][action_req].keys()[0]
                    self.action_goals[action_req] = self.config[namespace][action_req][goal_key].keys()
                    
                    # Capture ROS to MTConnect state conversion dictionary
                    #self.action_conv[action_req] = self.config[namespace][action_req]['conversion']
        return
    
    def xml_get_response(self, req):
        rospy.loginfo('Attempting HTTP connection on localhost:%s\tPort:%s' % (self.config['localhost'], self.config['Port']))
        self.conn.request("GET", req)
        response = self.conn.getresponse()
        if response.status != 200:
            rospy.loginfo("Request failed: %s - %d" % (response.reason, response.status))
            sys.exit(0)
        else:
            rospy.loginfo('Request --> %s, Status --> %s' % (response.reason, response.status))
        return response
    
    def xml_components(self, xml):
        """ Find all elements in the updated xml.
        root.find requires namespaces to be a dictionary.
        Return sequence and elements to process ROS msgs.
        """
        root = ElementTree.fromstring(xml)
        header = root.find('.//m:Header', namespaces=self.ns)
        nextSeq = header.attrib['nextSequence']
       
        elements = []

        for action in self.action_list:
            find_action = root.findall('.//m:' + action, namespaces=self.ns)
            if find_action: # Element list is not empty
                elements.append(find_action[0])
        
        return nextSeq, elements
    
    def action_client(self, cb_data):
        # Execute ROS Action
        rospy.init_node('ActionClient')
        
        # Unpack action client data
        name, goals, handle = cb_data
        rospy.loginfo('Launched %s Action CLient' % name)
        
        # Creates the SimpleActionClient, passing the type of the action (MaterialLoadAction) to the constructor.
        # i.e. name = 'MaterialLoad', action_type = mtconnect_msgs.msg.MaterialLoadAction
        action_type = getattr(handle, name + 'Action')
        client = actionlib.SimpleActionClient(name + 'Client', action_type)
        
        # Waits until the action server has started up and started listening for goals.
        rospy.loginfo('Waiting for %s Dedicated Action Server' % name)
        client.wait_for_server()
        rospy.loginfo('%s Dedicated Action Server Activated' % name)

        # Creates a MaterialLoad goal to send to the action server.
        # i.e. goal = mtconnect_msgs.msg.MaterialLoad()
        goal_handle = getattr(handle, name + 'Goal')
        goal = goal_handle()
        
        try:
            for attrib, xml_goal in zip(goal_handle.__slots__, self.xml_goal):
                setattr(goal, attrib, xml_goal)
        except Exception as e:
            rospy.logerr("ROS Action %s Client failed: %s" % (name, e))
        
        # Sends the goal to the action server.
        rospy.loginfo('Sending the goal')
        name_conv = self.split_event(name)
        client.send_goal(goal, done_cb = None, active_cb = self.action_cb(name_conv, 'ACTIVE'))
        
        # Waits for the server to finish performing the action.
        rospy.loginfo('Waiting for result')
        client.wait_for_result()
        
        # Obtain result
        result = client.get_result() # result must be a string
        
        # Prints out the result of the executing action
        rospy.loginfo('Returning the result --> %s' % result)
        
        # Set the Robot XML data item
        tokens = re.findall(r'([A-Z][a-z]*)', name)
        tokenlist = [val.lower() for val in tokens]
        data_item = tokenlist[0] + '_' + tokenlist[1]
        
        # Obtain text string for result -- Simulation Only, Replace with ROS-MTConnect conversion
        di_result = getattr(result, result.__slots__[0])
        
        # Submit converted result to host via MTConnect adapter
        self.action_cb(data_item, di_result)

        return

    def xml_callback(self, chunk):
        rospy.loginfo('*******************In PROCESS_XML callback***************')
        self.lock.acquire()
        try:
            # Only grab XML elements for CNC action requests 
            _, elements = self.xml_components(chunk)

            if elements:
                # Check for existing handshake, reverse elements if necessary
                if self.handshake != elements[0].attrib['name']:
                    # Reverse element order
                    elements = elements[::-1]
                for e in elements:
                    # Remove namespace string from the element tag for hash tables
                    action_text = re.findall(r'(?<=\})\w+',e.tag)[0]
                    
                    # Check if CNC is requesting an action, if so, run action client
                    if e.text == 'ACTIVE':
                        # Determine goal as specified in XML
                        response = self.xml_get_response("/cnc/current")
                        chunk = response.read()
                        root = ElementTree.fromstring(chunk)
                        
                        goal_tag = self.config[self.namespace][action_text].keys()[0]
                        goal_element = root.findall('.//m:' + goal_tag, namespaces=self.ns)[0]
                        self.xml_goal = (goal_element.text, 32.2, 7.4) # Should be a tuple of (material type, length, diameter)
                        self.action_client((action_text, self.action_goals[action_text], self.type_handle))
                        self.handshake = e.attrib['name']
                    # Check if CNC is submitting a handshake request
                    elif e.text == 'READY' and e.attrib['name'] == self.handshake:
                        # Send hand shake signal
                        self.action_cb(e.attrib['name'], 'READY')
                        self.handshake = None
        except Exception as e:
            rospy.logerr("Generic Action Client: Process XML callback failed: %s, releasing lock" % e)
        finally:
            self.lock.release()
        rospy.loginfo('*******************Done with PROCESS_XML callback***************')
        return
    
    def action_cb(self, data_item, state):
        # Respond that goal is accepted
        rospy.loginfo("Changing %s to '%s'" % (data_item, state))
        self.adapter.begin_gather()
        self.di_dict[data_item].set_value(state)
        self.adapter.complete_gather()
        return

    def ready_cb(self, chunk):
        _, elements = self.xml_components(chunk)
                
        if elements[0].text == 'READY':
            self.action_cb(elements[0].attrib['name'], 'READY')
        return
        


if __name__ == '__main__':
    try:
        rospy.loginfo('Starting Generic Action Client')
        robot_action = GenericActionClient()
    except rospy.ROSInterruptException:
        rospy.loginfo('program interrupted before completion')
		