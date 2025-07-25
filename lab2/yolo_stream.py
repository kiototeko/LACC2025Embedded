import cv2
from ultralytics import YOLO
import pdb

# 1. Configure
STREAM_URL = ""      # your camera’s MJPEG URL
MODEL_PATH = "yolov5s.pt"                       # or path to your custom .pt
DEVICE     = "cuda" if cv2.cuda.getCudaEnabledDeviceCount() else "cpu"

# 2. Load YOLOv5
model = YOLO(MODEL_PATH)       

# 3. Open the MJPEG stream
cap = cv2.VideoCapture(STREAM_URL)
if not cap.isOpened():
    raise RuntimeError(f"Couldn’t open stream: {STREAM_URL}")

# 4. Process loop
while True:
    ret, frame = cap.read()
    if not ret:
        break

    frame = cv2.rotate(frame, cv2.ROTATE_180)
    # 5. Run detection
    results = model(frame)
    results = results[0]

    # 6. Draw boxes
    for det in results.boxes:
        x1, y1, x2, y2 = map(int, det.xyxy[0])
        conf = det.conf[0]
        cls  = int(det.cls[0])
        label = f"{results.names[cls]} {conf:.2f}"
        cv2.rectangle(frame, (x1,y1), (x2,y2), (0,255,0), 2)
        cv2.putText(frame, label, (x1,y1-5),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0,255,0), 1)

    # 7. Show
    cv2.imshow("YOLO Detection", frame)
    if cv2.waitKey(1) == 27:  # ESC to quit
        break

cap.release()
cv2.destroyAllWindows()
