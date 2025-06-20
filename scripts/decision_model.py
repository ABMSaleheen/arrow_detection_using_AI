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

        self.subscriber_img = self.create_subscription(rosImage, 'autonomous_node/gray_scale_img', self.make_decision_callback, 4) 
        self.bridge = CvBridge()

        self.publisher_decision = self.create_publisher(Float32MultiArray, 'decision_node/detected_direction',5)
        # self.decision_pub_timer = self.create_timer(0.45, self.decision_pub_callback)

        self.class_num = -1.0 
        self.confidence_score = -1.0
        self.decision_msg = Float32MultiArray()

        self.img_rcvd_counter = 0
        # self.send_data == 0

    # def is_black_image(self, image):
    #     # Convert image to a NumPy array and check if all pixels are zero
    #     return np.all(np.array(image) == 0)


    def process_image_for_pil(self, image):
        # Load Image and process to input into the model -------------------
        # frame = self.bridge.imgmsg_to_cv2(image, desired_encoding="bgr8")
        frame = self.bridge.imgmsg_to_cv2(image)
        # print("Shape::  ", frame.shape)

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



    def make_decision_callback(self, image):
        # Disable scientific notation for clarity
        print("Counter Image Received 1: == ", self.img_rcvd_counter)

        print("\n >>>>>>>>>>>>>>> Image Received <<<<<<<")
        self.img_rcvd_counter += 1 # Add 1 when img is received for the 1st time
        # print("Counter Image Received 2: == ", self.img_rcvd_counter)
        

        np.set_printoptions(suppress=True)

        # Load the model
        model_file = os.path.join(package_share_dir,'training_models','keras_model3_L.h5')
        model = keras.models.load_model(model_file, compile=False)

        if not os.path.exists(model_file):
            raise FileNotFoundError(f"Model file not found: {model_file}")
        
        # load the labels
        model_labels_path = os.path.join(package_share_dir,'training_models','labels3_L.txt')
        class_names = open(model_labels_path, "r").readlines()

        # print("Labels:  ",class_names)

        # Create the array of the right shape to feed into the keras model
        # The 'length' or number of images you can put into the array is
        # determined by the first position in the shape tuple, in this case 1
        data = np.ndarray(shape=(1, 224, 224, 3), dtype=np.float32)

        if image.width == 0 or image.height == 0:
            self.get_logger().warn("Image has zero dimensions. Skipping processing.")
        ## Load Image and process to input into the model -------------------
            self.confidence_score = -1.0
            self.class_num= -1.0 


        else:
            print(">>>>>>>>>>>>>>> Image Process Started <<<<<<<")
            image = self.process_image_for_pil(image)

            # xxxxxxxxxxxxxxxxxxxxxxxxxxx

            # size = (224, 224)
            # # image = image.resize((224, 224), Image.LANCZOS)

            # image = ImageOps.fit(image, size, Image.Resampling.LANCZOS)

            # # turn the image into a numpy array
            # image_array = np.asarray(image)
            # # Normalize the image
            # normalized_image_array = (image_array.astype(np.float32) / 127.5) - 1
            # # normalized_image_array = image_array.astype(np.float32) / 255.0

            # # Load the image into the array
            # data[0] = normalized_image_array
            # # data[0] = image_array

            # xxxxxxxxxxxxxxxxxxxxxxxxxxx

            size = (224, 224)
            image = ImageOps.fit(image, size, Image.Resampling.LANCZOS)

            # turn the image into a numpy array
            image_array = np.asarray(image)

            # Normalize the image
            normalized_image_array = (image_array.astype(np.float32) / 127.5) - 1
            # normalized_image_array = image_array.astype(np.float32) / 255.0

            normalized1 = (image_array.astype(np.float32) / 127.5) - 1
            normalized2 = image_array.astype(np.float32) / 255.0

            # Compare model outputs
            pred1 = model.predict(np.expand_dims(normalized1, axis=0))
            pred2 = model.predict(np.expand_dims(normalized2, axis=0))

            print("Prediction with [-1,1] normalization:", pred1)
            print("Prediction with [0,1] normalization:", pred2)

            print("Image Shape:", image_array.shape, "Min:", np.min(image_array), "Max:", np.max(image_array))
            print("After Normalization - Min:", np.min(normalized_image_array), "Max:", np.max(normalized_image_array))


            # Load the image into the array
            data[0] = normalized_image_array

            # cv2.imwrite("/home/saleheen_linux/processed_image_cv2_norm.png", cv2.cvtColor(normalized_image_array, cv2.COLOR_RGB2BGR))
            # cv2.imwrite("/home/saleheen_linux/processed_image_cv2.png", cv2.cvtColor(image_array, cv2.COLOR_RGB2BGR))

            # Predicts the model
            prediction = model.predict(data)
            print("Raw Model Output:", prediction)
            index = np.argmax(prediction)
            class_name = class_names[index]

            self.confidence_score = float(prediction[0][index])
            self.class_num = float(class_name[0])   # 0: Left # 1: Right
                        
            self.img_rcvd_counter = 0
                

            print(">>>>>>>>>>>>>>> Image Process Ended - Decision Made   <<<<<<<")
            print("Class: == ", self.class_num, end="\n")
            print("Confidence Score: == ", self.confidence_score)

            self.decision_pub_callback()
            

            # # Print prediction and confidence score
            # print("Class:", class_name[2:], end="")
            # print("Confidence Score:", self.confidence_score)

            # self.decision_msg = Float32MultiArray()
            # self.decision_msg.data = [self.class_num, self.confidence_score]
            # self.publisher_decision.publish(self.decision_msg)

        # # displaying what is being recorded 
        # cv2.imshow("output_of_decision_node", image_array)
        # cv2.waitKey(1)

    # def send_decision(self, decision, accuracy):
    def decision_pub_callback(self):

        print("Class: == ", self.class_num, end=" /- Pubbed Decision\n")
        print("Confidence Score: == ", self.confidence_score)
        
        # self.img_rcvd_counter = 0
        
        # self.decision_msg.data = []

        self.decision_msg.data = [self.class_num, self.confidence_score]
        self.publisher_decision.publish(self.decision_msg)
        print(">>>>>>>>>>>>>>>  - Decision Published   <<<<<<<")



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
