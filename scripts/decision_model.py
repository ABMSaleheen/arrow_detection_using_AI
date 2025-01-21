#!/usr/bin/env python3
import os
from ament_index_python.packages import get_package_share_directory

# from keras.models import load_model
import keras
from PIL import Image, ImageOps

import rclpy
import cv2
from rclpy.node import Node
from cv_bridge import CvBridge
from sensor_msgs.msg import Image as rosImage
from std_msgs.msg import Float32MultiArray
import numpy as np


package_name = 'arrow_detection'
package_share_dir = get_package_share_directory(package_name)

class DecisionMakingNode(Node):
    def __init__(self):
        super().__init__("decision_making_node")
        self.get_logger().info("Decision is being made............")

        self.subscriber_img = self.create_subscription(rosImage, 'autonomous_node/gray_scale_img', self.make_decision_callback, 10) 
        self.bridge = CvBridge()

        self.publisher_decision = self.create_publisher(Float32MultiArray, 'decision_node/detected_direction', 10)

    def process_image_for_pil(self, image):
        # Load Image and process to input into the model -------------------
        frame = self.bridge.imgmsg_to_cv2(image)
        print("Shape::  ", frame.shape)

        # Assuming 'frame' is a NumPy array from cv_bridge
        try:
            # Convert NumPy array to PIL Image
            if len(frame.shape) == 3:  # Color image (BGR format from OpenCV)
                image = Image.fromarray(cv2.cvtColor(frame, cv2.COLOR_BGR2RGB))  # Convert BGR to RGB
            elif len(frame.shape) == 2:  # Grayscale image
                image = Image.fromarray(frame).convert("RGB")  # Convert grayscale to RGB
            else:
                raise ValueError("Unsupported frame format")

            # Further processing with PIL
            # image.show()  # Example: Display the image
        except Exception as e:
            print(f"Error during image processing: {e}")

        return image


    # def publish_decision(self, msg):
    #     self.publish_decision(msg)

    def make_decision_callback(self, image):
        # Disable scientific notation for clarity
        np.set_printoptions(suppress=True)

        # Load the model
        model_file = os.path.join(package_share_dir,'training_models','keras_model.h5')
        model = keras.models.load_model(model_file, compile=False)

        if not os.path.exists(model_file):
            raise FileNotFoundError(f"Model file not found: {model_file}")
        
        # load the labels
        model_labels_path = os.path.join(package_share_dir,'training_models','labels.txt')
        class_names = open(model_labels_path, "r").readlines()

        print("Labels:  ",class_names)

        # Create the array of the right shape to feed into the keras model
        # The 'length' or number of images you can put into the array is
        # determined by the first position in the shape tuple, in this case 1
        data = np.ndarray(shape=(1, 224, 224, 3), dtype=np.float32)

        ## Load Image and process to input into the model -------------------
        image = self.process_image_for_pil(image)

        size = (224, 224)
        image = ImageOps.fit(image, size, Image.Resampling.LANCZOS)

        # turn the image into a numpy array
        image_array = np.asarray(image)
        # Normalize the image
        normalized_image_array = (image_array.astype(np.float32) / 127.5) - 1
        # Load the image into the array
        data[0] = normalized_image_array

        # Predicts the model
        prediction = model.predict(data)
        index = np.argmax(prediction)
        class_name = class_names[index]
        confidence_score = float(prediction[0][index])

        class_num= float(class_name[0])   # 0: Left # 1: Right

        # Print prediction and confidence score
        print("Class:", class_name[2:], end="")
        print("Confidence Score:", confidence_score)

        decision_msg = Float32MultiArray()
        decision_msg.data = [class_num, confidence_score]

        self.publisher_decision.publish(decision_msg)

        # # displaying what is being recorded 
        # cv2.imshow("output_of_decision_node", frame)
        # cv2.waitKey(1)

    # def send_decision(self, decision, accuracy):



def main(args=None):
    rclpy.init(args=args)
    decision_node = DecisionMakingNode()


    try:
        rclpy.spin(decision_node)
    except KeyboardInterrupt:
        decision_node.get_logger().info("Shutting down AI Node.")
    finally:
        decision_node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()
