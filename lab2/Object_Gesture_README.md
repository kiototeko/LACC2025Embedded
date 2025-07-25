# YOLO Stream Detection

## What This Code Does

This Python script connects to a camera's video stream, uses a YOLO ("You Only Look Once") object detection model to find objects in each video frame, draws boxes and labels around detected objects, and displays the video in real time.

## Prerequisites

To run this code, you need:

* Python 3 installed on your computer.
* The following Python libraries:

  * `opencv-python` (for handling video and drawing)
  * `ultralytics` (for the YOLO model)
* A working MJPEG camera stream URL (e.g., from an IP camera).
* A YOLO model file (`.pt`), such as `yolov5s.pt`.

You can install the libraries with:

```bash
pip install opencv-python ultralytics
```

## How to Use

1. Open the file `yolo_stream.py` in your text editor.
2. Set the `STREAM_URL` variable to your camera's MJPEG URL.
3. (Optional) Change `MODEL_PATH` if you have a different `.pt` file.
4. Run the script:

   ```bash
   python yolo_stream.py
   ```
5. A window will open showing the live video with detected objects.
6. Press **ESC** on your keyboard to stop and close the window.

## Code Walkthrough

Below is a breakdown of the most important parts of the code. We include small snippets to illustrate each section.

### 1. Configuration

```python
STREAM_URL = ""      # your camera’s MJPEG URL
MODEL_PATH = "yolov5s.pt"  # path to the YOLO model file
```

* **STREAM\_URL**: Replace `""` with the URL of your camera stream.
* **MODEL\_PATH**: Path to the pre-trained YOLO model (e.g., `yolov5s.pt`).

### 2. Loading the YOLO Model

```python
model = YOLO(MODEL_PATH)
```

* This line loads the YOLO model so we can use it for detection.

### 3. Opening the Video Stream

```python
cap = cv2.VideoCapture(STREAM_URL)
if not cap.isOpened():
    raise RuntimeError(f"Couldn’t open stream: {STREAM_URL}")
```

* `cv2.VideoCapture` connects to the video stream.
* We check if the connection succeeded and show an error otherwise.

### 4. Processing Loop

```python
while True:
    ret, frame = cap.read()
    if not ret:
        break
    frame = cv2.rotate(frame, cv2.ROTATE_180)
    # ...
```

* This `while` loop runs forever (until you stop it).
* `cap.read()` reads one frame at a time.
* We rotate the frame upside down to correct camera orientation.

### 5. Running Object Detection

```python
results = model(frame)
results = results[0]
```

* We pass each video frame to `model(frame)`.
* The model returns a list of results; we take the first one.

### 6. Drawing Bounding Boxes

```python
for det in results.boxes:
    x1, y1, x2, y2 = map(int, det.xyxy[0])
    conf = det.conf[0]
    cls  = int(det.cls[0])
    label = f"{results.names[cls]} {conf:.2f}"
    cv2.rectangle(frame, (x1,y1), (x2,y2), (0,255,0), 2)
    cv2.putText(frame, label, (x1,y1-5),
                cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0,255,0), 1)
```

* Loop through each detection (`det`) in the frame.
* Extract coordinates (`x1, y1, x2, y2`), confidence score (`conf`), and class ID (`cls`).
* Draw a green rectangle around the object.
* Put a text label showing the object name and confidence.

### 7. Displaying the Video

```python
cv2.imshow("YOLO Detection", frame)
if cv2.waitKey(1) == 27:  # ESC to quit
    break

cap.release()
cv2.destroyAllWindows()
```

* `cv2.imshow` opens a window showing the annotated frame.
* `cv2.waitKey(1)` waits for 1 millisecond; if you press **ESC** (key code 27), it exits the loop.
* Finally, we release the camera and close all OpenCV windows.

---

# Gesture Detection with MediaPipe

## What This Code Does

This Python script connects to a camera's video stream, tracks your hand using MediaPipe, recognizes a simple gesture ("thumbs up"), and displays the video with the tracking landmarks and gesture label in real time.

## Prerequisites

To run this code, you need:

* Python 3 installed on your computer.
* The following Python libraries:

  * `opencv-python` (for handling video and drawing)
  * `mediapipe` (for hand tracking and landmarks)
* A working MJPEG camera stream URL (e.g., from an IP camera) or a webcam index.

You can install the libraries with:

```bash
pip install opencv-python mediapipe
```

## How to Use

1. Open the file `gesture.py` in your text editor.
2. Set the `STREAM_URL` variable to your camera's MJPEG URL or leave it empty (default webcam).
3. Run the script:

   ```bash
   python gesture.py
   ```
4. A window will open showing the live video with hand landmarks and the detected gesture.
5. Press **ESC** on your keyboard to stop and close the window.

## Code Walkthrough

Below is a breakdown of the most important parts of the code. We include small snippets to illustrate each section.

### 1. Setup and Configuration

```python
STREAM_URL = ""    # your camera’s MJPEG URL, or leave empty for default webcam
cap = cv2.VideoCapture(STREAM_URL)
```

* **STREAM\_URL**: Replace `""` with the URL of your camera stream, or leave it empty (`""`) to use your computer’s default webcam.
* **cap**: Connects to the video stream.

### 2. Hand Tracking with MediaPipe

```python
mp_hands = mp.solutions.hands
hands = mp_hands.Hands(
    static_image_mode=False,
    max_num_hands=1,
    min_detection_confidence=0.5,
    min_tracking_confidence=0.5
)
mp_draw = mp.solutions.drawing_utils
```

* **mp\_hands.Hands()**: Sets up the hand-tracking model.

  * `static_image_mode=False`: Treats input as video.
  * `max_num_hands=1`: Tracks at most one hand.
  * Confidence values control when detection and tracking run.
* **mp\_draw**: Provides utilities to draw landmarks and connections.

### 3. Gesture Classification

```python
def classify_gesture(landmarks):
    thumb_tip_y = landmarks[4].y
    thumb_mcp_y = landmarks[2].y
    fingers_folded = all(
      landmarks[i].y > landmarks[i-2].y
      for i in [8, 12, 16, 20]
    )
    if thumb_tip_y < thumb_mcp_y and fingers_folded:
      return "Thumbs Up"
    return "Unknown"
```

* **landmarks**: A list of 21 points (x, y, z) for each part of the hand.
* We check if the thumb tip is above its base and all other fingertips are folded down.
* Returns a label: **"Thumbs Up"** or **"Unknown"**.

### 4. Processing Loop

```python
while True:
    ret, frame = cap.read()
    if not ret:
        break

    frame = cv2.rotate(frame, cv2.ROTATE_180)
    rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
    results = hands.process(rgb)

    gesture_label = ""
    if results.multi_hand_landmarks:
        for handLms in results.multi_hand_landmarks:
            mp_draw.draw_landmarks(frame, handLms, mp_hands.HAND_CONNECTIONS)
            gesture_label = classify_gesture(handLms.landmark)
```

* Reads frames in a loop until the stream ends or you quit.
* Rotates the frame if your camera is upside down.
* Converts BGR color (OpenCV) to RGB (MediaPipe requirement).
* Processes the frame to find hand landmarks.
* Draws landmarks and classifies the gesture.

### 5. Drawing and Displaying

```python
    cv2.putText(frame, gesture_label, (10,20),
                cv2.FONT_HERSHEY_SIMPLEX, 0.4, (0,255,0), 1)
    cv2.imshow("Gesture Detection", frame)
    if cv2.waitKey(1) & 0xFF == 27:  # ESC to quit
        break

cap.release()
cv2.destroyAllWindows()
```

* **cv2.putText**: Overlays the detected gesture label on the video.
* **cv2.imshow**: Displays the video window.
* Press **ESC** to exit the loop.
* **cap.release()** and **cv2.destroyAllWindows()** clean up resources.



