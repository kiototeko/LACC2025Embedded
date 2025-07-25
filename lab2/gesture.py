import cv2
import mediapipe as mp

# 1) Setup
STREAM_URL = ""
cap = cv2.VideoCapture(STREAM_URL)

mp_hands = mp.solutions.hands
hands    = mp_hands.Hands(
    static_image_mode=False,
    max_num_hands=1,
    min_detection_confidence=0.5,
    min_tracking_confidence=0.5
)
mp_draw  = mp.solutions.drawing_utils

def classify_gesture(landmarks):
    # landmarks is a list of 21 (x,y,z) tuples
    # Example: simple “thumbs up” test: 
    #    tip of thumb above MCP joint, all other fingertips below their MCP
    thumb_tip_y = landmarks[4].y
    thumb_mcp_y = landmarks[2].y
    fingers_folded = all(
      landmarks[i].y > landmarks[i-2].y 
      for i in [8, 12, 16, 20]
    )
    if thumb_tip_y < thumb_mcp_y and fingers_folded:
      return "Thumbs Up"
    return "Unknown"

# 2) Processing loop
while True:
    ret, frame = cap.read()
    if not ret:
        break

    # flip if needed

    frame = cv2.rotate(frame, cv2.ROTATE_180)

    # convert color for MediaPipe
    rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
    results = hands.process(rgb)

    gesture_label = ""
    if results.multi_hand_landmarks:
        for handLms in results.multi_hand_landmarks:
            # draw the hand skeleton
            mp_draw.draw_landmarks(frame, handLms, mp_hands.HAND_CONNECTIONS)
            # classify
            gesture_label = classify_gesture(handLms.landmark)

    # overlay label
    cv2.putText(frame, gesture_label, (10,20),
                cv2.FONT_HERSHEY_SIMPLEX, 0.4, (0,255,0), 1)

    cv2.imshow("Gesture Detection", frame)
    if cv2.waitKey(1) & 0xFF == 27:  # ESC to quit
        break

cap.release()
cv2.destroyAllWindows()
