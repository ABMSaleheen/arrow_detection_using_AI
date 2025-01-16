
import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from launch.actions import ExecuteProcess

rviz = 0

def generate_launch_description():
    package_dir = get_package_share_directory('arrow_detection')  # pkg dir after building in install/share
    urdf = os.path.join(package_dir,'urdf','rover_proto_A1','rover_proto_A1.urdf')   # urdf is saved here in install/share after build
    
    urdf_rviz = os.path.join(package_dir,'urdf','rover_proto_A1', 'rover_proto_A1_rviz.urdf')
    rviz_config_file=os.path.join(package_dir, 'urdf','rover_proto_A1', 'config.rviz') # config.rviz is saved here in install/share after build
    
    # print("pkg rover location:",urdf)


    # world_file = 'track_1.world' # for right wall following
    world_file = 'track_2.world' 
    world_path = os.path.join(package_dir,'worlds', world_file) 

    return LaunchDescription([

        DeclareLaunchArgument(
            'use_rviz',
            default_value= '0',
            description='Set to "1" to launch RViz'
        ),

        Node(
            package='robot_state_publisher',
            executable='robot_state_publisher',
            name='robot_state_publisher',
            output='screen',
            arguments=[urdf_rviz],
            parameters=[{'use_sim_time':True}]
            ),

        Node(
            package='joint_state_publisher',
            executable='joint_state_publisher',
            name='joint_state_publisher',
            arguments=[urdf_rviz]),



        Node(
            package='rviz2',
            executable='rviz2',
            name='rviz2',
            arguments=['-d',rviz_config_file],
            output='screen',
            condition = IfCondition(LaunchConfiguration('use_rviz'))
            ),




        ExecuteProcess(
            cmd=['gazebo', '--verbose','-s','libgazebo_ros_init.so','-s', 'libgazebo_ros_factory.so', world_path],
            output='screen'
        ),


        Node(
            package='gazebo_ros',
            executable='spawn_entity.py',
            name='spawn_entity',
            arguments=["-topic", "/robot_description", "-entity", "rover_proto_A1_rviz"],
            # arguments=['-file', urdf_rviz, '-name', 'rover_proto_A1_rviz'], # changed to urdf_rviz from urdf
            output='screen'
        ),

        # Node(
        #     package='joint_state_publisher_gui',
        #     executable='joint_state_publisher_gui',
        #     name='joint_state_publisher_gui',
        #     arguments=[urdf_rviz]),


    ])

