import face_recognition
import os
import pickle

def get_face_encodings(image_dir):
    face_encodings = []
    face_names = []

    for person_name in os.listdir(image_dir):
        person_dir = os.path.join(image_dir, person_name)
        if not os.path.isdir(person_dir):
            continue

        for img_name in os.listdir(person_dir):
            img_path = os.path.join(person_dir, img_name)
            image = face_recognition.load_image_file(img_path)
            encodings = face_recognition.face_encodings(image)
            if encodings:
                face_encodings.append(encodings[0])
                face_names.append(person_name)

    return face_encodings, face_names

# Directory containing captured images
image_dir = "dataset"

# Get face encodings
face_encodings, face_names = get_face_encodings(image_dir)

# Save the encodings and names
with open("face_encodings.pkl", "wb") as f:
    pickle.dump((face_encodings, face_names), f)

print("Model trained and saved.")