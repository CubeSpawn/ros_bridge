# Copyright 2012, System Insights, Inc.
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.

$: << '.'

require 'rubygems'
require 'adapter'
require 'statemachine'
require 'statemachine/generate/dot_graph'
require 'mtc_context'
require 'chuck'

module Cnc
  class CncContext < MTConnect::Context  
    include MTConnect
    attr_accessor :robot_controller_mode, :robot_material_load, :robot_material_unload, :robot_controller_mode,
                  :robot_open_chuck, :robot_close_chuck, :robot_open_door, :robot_close_door
    attr_reader :adapter, :chuck_state, :open_chuck, :close_chuck, :door_state, :open_door, :close_door
  
    def initialize(port = 7879)
      super(port)
      
      @adapter.data_items << (@availability = DataItem.new('avail'))

      @adapter.data_items << (@material_load = DataItem.new('material_load'))
      @adapter.data_items << (@material_unload = DataItem.new('material_unload'))

      @adapter.data_items << (@open_chuck = DataItem.new('open_chuck'))
      @adapter.data_items << (@close_chuck = DataItem.new('close_chuck'))

      @adapter.data_items << (@open_door = DataItem.new('open_door'))
      @adapter.data_items << (@close_door = DataItem.new('close_door'))


      @adapter.data_items << (@chuck_state = DataItem.new('chuck_state'))

      @adapter.data_items << (@link = DataItem.new('robo_link'))
      @adapter.data_items << (@exec = DataItem.new('exec'))
      @adapter.data_items << (@mode = DataItem.new('mode'))
      @adapter.data_items << (@door_state = DataItem.new('door_state'))

      @adapter.data_items << (@material = DataItem.new('material'))


      @adapter.data_items << (@system = SimpleCondition.new('system'))

      @interfaces = [@open_door, @open_chuck, @close_door, @close_chuck,
                     @material_load, @material_unload]

      # Initialize data items
      @availability.value = "AVAILABLE"
      @chuck_state.value = 'OPEN'
      @link.value = 'ENABLED'
      @exec.value = 'READY'
      @mode.value = 'AUTOMATIC'
      @door_state.value = "OPEN"

      @material.value = 'ROUND 440C THING'

      @system.normal

      @open_chuck_interface = OpenChuck.new(self)
      @close_chuck_interface = CloseChuck.new(self)

      @adapter.start    
    end
  
    def stop
      @adapter.stop
    end

    def event(name, value)
      puts "CNC Received #{name} #{value}"
      case name
      when "OpenChuck"
        action = value.downcase.to_sym
        @open_chuck_interface.statemachine.send(action)

      when "CloseChuck"
        action = value.downcase.to_sym
        @close_chuck_interface.statemachine.send(action)

      else
        super(name, value)
      end
    end
    
    def cnc_not_ready
      @adapter.gather do
        @open_chuck_interface.statemachine.not_ready
        @close_chuck_interface.statemachine.not_ready
      end
    end

    def cnc_ready
      puts "****** Beginning New Part ******"

      @adapter.gather do
        @exec.value = 'READY'
      end
      @open_chuck_interface.statemachine.ready
      @close_chuck_interface.statemachine.ready
      @statemachine.handling
    end

    def activate
      if @faults.empty? and @mode.value == 'AUTOMATIC' and
          @link.value == 'ENABLED' and @robot_controller_mode == 'AUTOMATIC' and
          @robot_material_load == 'READY' and @robot_material_unload == 'READY'
        puts "Becomming operational"
        @open_chuck_interface.statemachine.ready
        @close_chuck_interface.statemachine.ready
        @statemachine.make_operational
      else
        puts "Still not ready"
        @statemachine.still_not_ready
      end
    end
    
    def automatic_mode
      @adapter.gather do
        @mode.value = 'AUTOMATIC'
      end
    end
    
    def manual_mode
      @adapter.gather do
        @mode.value = 'MANUAL'
      end
    end

    def enable
      @adapter.gather do
        @link.value = 'ENABLED'
      end
    end

    def disable
      @adapter.gather do
        @link.value = 'DISABLED'
      end
    end

    def cycling
      @adapter.gather do
        @exec.value = 'ACTIVE'
        @material_load.value = 'READY'
      end
      Thread.new do
        puts "------ Cutting a part ------"
        sleep 5
        puts "------ Finished Cutting a part ------"
        @statemachine.cycle_complete
      end
    end

    def cycle_complete
      @adapter.gather do
        @exec.value = 'READY'
      end
    end

    def material_load
      @adapter.gather do
        @material_load.value = 'ACTIVE'
      end
    end

    def material_load_ready
      @adapter.gather do
        @material_load.value = 'READY'
      end
    end

    def material_unload
      @adapter.gather do
        @material_unload.value = 'ACTIVE'
      end
    end

    def material_unload_ready
      @adapter.gather do
        @material_unload.value = 'READY'
      end
    end

    def one_cycle
      # Make sure spindle is not latched
      @adapter.gather do
        @load_material.value = 'READY'
      end
      Thread.new do
        sleep 1
        @statemachine.cycle_completed
      end
    end

    def unload_failed
      @adapter.gather do
        @material_unload.value = 'FAIL'
      end
    end
    
    def reset_history
      @statemachine.get_state(:operational).reset
    end
  
    def add_conditions
    end  
  end

  @cnc = Statemachine.build do
    startstate :disabled
  
    superstate :base do  
      event :fault, :fault, :reset_history

      # From the robot
      event :robot_availability_unavailable, :activated
      event :robot_controller_mode_automatic, :activated
      event :robot_material_load_ready, :activated
      event :robot_material_load_not_ready, :activated
      event :robot_material_unload_not_ready, :activated
      event :robot_material_unload_ready, :activated

      # command lines
      event :auto, :activated, :automatic_mode
      event :maunal, :activated, :manual_mode
      event :disable, :activated, :disable
      event :enable, :activated, :enable
      event :normal, :activated

      superstate :disabled do
        default :not_ready
        default_history :not_ready
        on_entry :cnc_not_ready
    
        # Ways to transition out of not ready state...
        state :not_ready do
          on_entry :cnc_not_ready
          default :not_ready          
        end
  
        state :fault do
          on_entry :cnc_not_ready
          event :normal, :activated
        end
      end
  
      state :activated do
        on_entry :activate
        event :make_operational, :operational_H
        event :still_not_ready, :not_ready
      end
    
      superstate :operational do
        startstate :ready
        default_history :ready
        event :robot_controller_mode_manual, :activated, :reset_history
        event :robot_controller_mode_manual_data_input, :activated, :reset_history
        event :robot_material_unload_fail, :material_unload_failed
        
        state :ready do
          default :ready
          on_entry :cnc_ready
          event :handling, :handling
        end
        
        state :cycle_start do
          on_entry :cycling
          event :cycle_complete, :material_unload, :cycle_complete
        end

        superstate :handling do
          default_history :material_load

          state :material_load do
            on_entry :material_load
            event :robot_material_load_active, :material_load
            event :robot_material_load_complete, :cycle_start
            event :robot_material_load_not_ready, :activated, :reset_history
          end

          state :material_unload do
            on_entry :material_unload
            event :robot_material_unload_active, :material_unload
            event :robot_material_unload_complete, :ready
            event :robot_material_unload_not_ready, :activated, :reset_history
          end

          state :material_unload_failed do
            on_entry :unload_failed
            default :material_unload_failed
            event :robot_material_unload_fail, :activated, :reset_history
            event :robot_material_unload_ready, :activated, :reset_history
          end
        end
      end
    end
    
    context CncContext.new    
  end

  def self.cnc
    @cnc
  end
end

if $0 == __FILE__
  module Statemachine
    module Generate
      module DotGraph
        class DotGraphStatemachine
          def explore_sm
            @nodes = []
            @transitions = []
            @sm.states.values.each { |state|
              state.transitions.values.each { |transition|
                @nodes << transition.origin_id
                @nodes << transition.destination_id
                @transitions << transition
              }
            }
            @transitions = @transitions.uniq
            @nodes = @nodes.uniq
          end
        end
      end
    end
  end
  dir = File.dirname(__FILE__) + '/graph'
  Dir.mkdir dir unless File.exist?(dir)
  Cnc.cnc.to_dot(:output => 'graph')
  Dir.chdir('graph') do
    system('dot -Tpng -o main.png main.dot')
    Dir['*.dot'].each do |f|
      system("dot -Tsvg -o #{File.basename(f, '.dot')}.svg #{f}")
    end
  end
end