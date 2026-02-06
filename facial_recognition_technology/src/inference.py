import cv2
import face_recognition
import numpy as np
from picamera2 import Picamera2
import pickle
import serial
import time

# Load pre-trained face encodings
with open("face_encodings.pkl", "rb") as f:
    known_face_encodings, known_face_names = pickle.load(f)

# Initialize the camera
picam2 = Picamera2()
picam2.configure(picam2.create_preview_configuration(main={"format": 'XRGB8888', "size": (1920, 1080)}))
picam2.start()

# Initialize Serial Port
arduino = serial.Serial(port='/dev/ttyS0', baudrate=9600)

cv_scaler = 5   # Scale factor for resizing frames
face_locations = []
face_encodings = []
face_names = []
frame_count = 0
start_time = time.time()
fps = 0

authorized_names = ['Bryan']

# Compares the face encodings of the faces in the camera's frame with the known face encodings
def process_frame(frame):
    global face_locations, face_encodings, face_names
    resized_frame = cv2.resize(frame, (0, 0), fx=(1/cv_scaler), fy=(1/cv_scaler))
    rgb_resized_frame = cv2.cvtColor(resized_frame, cv2.COLOR_BGR2RGB)

    # Find all the faces and face encodings in the current frame of video
    face_locations = face_recognition.face_locations(rgb_resized_frame)
    face_encodings = face_recognition.face_encodings(rgb_resized_frame, face_locations, model='large')

    face_names = []
    authorized = False

    # Loop through each face found in the frame and check if it matches any known faces
    if face_encodings:
        for face_encoding in face_encodings:
            matches = face_recognition.compare_faces(known_face_encodings, face_encoding)

            # Caclulate the index by using the known face encodings with the new face encodings
            face_distances = face_recognition.face_distance(known_face_encodings, face_encoding)
            best_match_index = np.argmin(face_distances)

            if matches[best_match_index]:
                name = known_face_names[best_match_index]
            else:
                name = "Unknown"
            
            face_names.append(name)
            
            if name in authorized_names:
                authorized = True

        # Send message to Arduino based on detection status
        if authorized:
            arduino.write(b'IDENTIFIED\n')    
        else:
            arduino.write(b'Unknown\n')
    return frame

def display(frame):
    for (top, right, bottom, left), name in zip(face_locations, face_names):
        # Scale back up face locations since the frame we detected in was scaled
        top *= cv_scaler
        right *= cv_scaler
        bottom *= cv_scaler
        left *= cv_scaler

        # Draw a box around the face
        cv2.rectangle(frame, (left, top), (right, bottom), (244, 42, 3), 3)

        # Draw a label with a name below the face
        cv2.rectangle(frame, (left -3, top - 35), (right+3, top), (244, 42, 3), cv2.FILLED)
        font = cv2.FONT_HERSHEY_DUPLEX
        cv2.putText(frame, name, (left + 6, top - 6), font, 1.0, (255, 255, 255), 1)

        # Add an indicator if the person is authorized
        if name in authorized_names:
            cv2.putText(frame, "Authorized", (left + 6, bottom + 23), font, 0.6, (0, 255, 0), 1)
    return frame

def calculate_fps():
    global frame_count, start_time, fps
    frame_count += 1
    elapsed_time = time.time() - start_time
    if elapsed_time > 1:
        fps = frame_count / elapsed_time
        frame_count = 0
        start_time = time.time()
    return fps

while True:
    frame = picam2.capture_array()
    processed_frame = process_frame(frame)
    display_frame = display(processed_frame)
    current_fps = calculate_fps()

    cv2.putText(display_frame, f"FPS: {current_fps:.2f}", (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
    cv2.imshow("Camera", display_frame)

    key = cv2.waitKey(1) & 0xFF
    if key == ord("q"):
        break

cv2.destroyAllWindows()
picam2.stop()