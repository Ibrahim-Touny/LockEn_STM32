#!/usr/bin/env python3
"""
Face recognition worker — spawned once by server.js, communicates via stdin/stdout JSON lines.

Install:
    pip install insightface onnxruntime opencv-python

On first run, InsightFace downloads model files (~100 MB) to ~/.insightface/models/

Protocol:
  IN  {"cmd":"reset"}                        -> {"result":"ok"}
  IN  {"cmd":"enroll",    "image":"<b64>"}   -> {"result":"enrolled"} | {"result":"no_face"} | {"result":"error"}
  IN  {"cmd":"recognize", "image":"<b64>"}   -> {"result":"match"}    | {"result":"no_match"} | {"result":"no_face"} | {"result":"error"}
"""
import sys
import json
import base64
import io
import os

import numpy as np
import cv2
from PIL import Image
from insightface.app import FaceAnalysis

EMBEDDING_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'face_embedding.npy')
THRESHOLD = 0.50   # cosine similarity — raise to 0.60 if too many false positives

app = None

def init():
    global app
    # buffalo_sc = lighter (~100 MB); swap to 'buffalo_l' for higher accuracy (~500 MB)
    app = FaceAnalysis(name='buffalo_sc', providers=['CPUExecutionProvider'])
    app.prepare(ctx_id=0, det_size=(320, 320))

def send(obj):
    print(json.dumps(obj), flush=True)

def decode_image(b64_str):
    """Decode base64 JPEG → BGR numpy array (InsightFace expects BGR)."""
    img_bytes = base64.b64decode(b64_str)
    img = Image.open(io.BytesIO(img_bytes)).convert('RGB')
    arr = np.array(img)
    return cv2.cvtColor(arr, cv2.COLOR_RGB2BGR)

def get_embedding(img_bgr):
    """Return L2-normalised 512-dim embedding, or None if no face detected."""
    faces = app.get(img_bgr)
    if not faces:
        return None
    # pick the largest face in frame
    face = max(faces, key=lambda f: (f.bbox[2] - f.bbox[0]) * (f.bbox[3] - f.bbox[1]))
    return face.normed_embedding   # already normalised → dot product = cosine similarity

def main():
    sys.stderr.write('[face_recog] Initialising InsightFace...\n')
    sys.stderr.flush()
    init()
    sys.stderr.write('[face_recog] Ready\n')
    sys.stderr.flush()

    for raw_line in sys.stdin:
        raw_line = raw_line.strip()
        if not raw_line:
            continue
        try:
            req = json.loads(raw_line)
        except Exception:
            send({'result': 'error', 'msg': 'bad json'})
            continue

        cmd = req.get('cmd')

        if cmd == 'reset':
            if os.path.exists(EMBEDDING_FILE):
                os.remove(EMBEDDING_FILE)
            send({'result': 'ok'})

        elif cmd == 'enroll':
            try:
                img = decode_image(req['image'])
                emb = get_embedding(img)
                if emb is None:
                    send({'result': 'no_face'})
                else:
                    np.save(EMBEDDING_FILE, emb)
                    send({'result': 'enrolled'})
            except Exception as e:
                send({'result': 'error', 'msg': str(e)})

        elif cmd == 'recognize':
            try:
                if not os.path.exists(EMBEDDING_FILE):
                    send({'result': 'no_match'})
                    continue
                stored = np.load(EMBEDDING_FILE)
                img    = decode_image(req['image'])
                emb    = get_embedding(img)
                if emb is None:
                    send({'result': 'no_face'})
                    continue
                similarity = float(np.dot(stored, emb))   # cosine similarity (0–1)
                send({'result': 'match' if similarity > THRESHOLD else 'no_match'})
            except Exception as e:
                send({'result': 'error', 'msg': str(e)})

        else:
            send({'result': 'error', 'msg': 'unknown cmd'})

if __name__ == '__main__':
    main()
