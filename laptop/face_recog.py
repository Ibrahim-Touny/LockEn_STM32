#!/usr/bin/env python3
"""
Face recognition worker — spawned once by server.js, communicates via stdin/stdout JSON lines.

Protocol:
  IN  {"cmd":"reset"}
  IN  {"cmd":"enroll",    "image_path":"..."}  -> progress until enough samples, then enrolled
  IN  {"cmd":"recognize", "image_path":"..."}  -> match | no_match | no_face | error
"""
import sys
import json
import os

import numpy as np
import cv2
from insightface.app import FaceAnalysis

EMBEDDING_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'face_embedding.npy')
THRESHOLD = 0.32
MIN_DET_SCORE = 0.40
ENROLL_SAMPLES_REQUIRED = 4

app = None
enroll_samples = []

_protocol_out = os.fdopen(os.dup(1), 'w', encoding='utf-8', buffering=1, closefd=True)
sys.stdout = sys.stderr


def log(msg):
    sys.stderr.write(msg + '\n')
    sys.stderr.flush()


def send(obj):
    _protocol_out.write(json.dumps(obj) + '\n')
    _protocol_out.flush()


def init():
    global app
    app = FaceAnalysis(name='buffalo_sc', providers=['CPUExecutionProvider'])
    app.prepare(ctx_id=0, det_size=(640, 640), det_thresh=0.30)


def load_image(path):
    if not path or not os.path.exists(path):
        raise FileNotFoundError(f'image not found: {path}')
    data = np.fromfile(path, dtype=np.uint8)
    img = cv2.imdecode(data, cv2.IMREAD_COLOR)
    if img is None:
        raise ValueError('cv2.imdecode failed')
    return img


def prepare_frame(img_bgr):
    """ESP32-CAM front sensor is mirrored — always un-mirror for consistency."""
    img_bgr = cv2.flip(img_bgr, 1)
    h, w = img_bgr.shape[:2]
    longest = max(h, w)
    if longest < 640:
        scale = 640 / float(longest)
        img_bgr = cv2.resize(
            img_bgr,
            (int(w * scale), int(h * scale)),
            interpolation=cv2.INTER_LINEAR,
        )
    return img_bgr


def get_embedding(img_bgr):
    prepared = prepare_frame(img_bgr)
    faces = app.get(prepared)
    if not faces:
        return None, 0.0, 0

    face = max(faces, key=lambda f: (f.bbox[2] - f.bbox[0]) * (f.bbox[3] - f.bbox[1]))
    score = float(getattr(face, 'det_score', 1.0))
    if score < MIN_DET_SCORE:
        return None, score, len(faces)
    return face.normed_embedding, score, len(faces)


def save_average_embedding():
    global enroll_samples
    if len(enroll_samples) < ENROLL_SAMPLES_REQUIRED:
        return False
    avg = np.mean(np.stack(enroll_samples), axis=0)
    avg = avg / np.linalg.norm(avg)
    np.save(EMBEDDING_FILE, avg)
    enroll_samples = []
    return True


def main():
    global enroll_samples
    log('[face_recog] Initialising InsightFace...')
    init()
    log('[face_recog] Ready')

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
            enroll_samples = []
            if os.path.exists(EMBEDDING_FILE):
                os.remove(EMBEDDING_FILE)
            send({'result': 'ok'})

        elif cmd == 'enroll':
            try:
                img = load_image(req.get('image_path'))
                emb, score, face_count = get_embedding(img)
                h, w = img.shape[:2]
                log(f'[face_recog] enroll: {w}x{h} faces={face_count} score={score:.2f}')

                if emb is None:
                    send({'result': 'no_face'})
                    continue

                enroll_samples.append(emb)
                count = len(enroll_samples)
                log(f'[face_recog] enroll sample {count}/{ENROLL_SAMPLES_REQUIRED}')

                if count >= ENROLL_SAMPLES_REQUIRED:
                    save_average_embedding()
                    send({'result': 'enrolled', 'samples': count})
                else:
                    send({
                        'result': 'progress',
                        'count': count,
                        'needed': ENROLL_SAMPLES_REQUIRED,
                    })
            except Exception as e:
                log(f'[face_recog] error: {e}')
                send({'result': 'error', 'msg': str(e)})

        elif cmd == 'recognize':
            try:
                if not os.path.exists(EMBEDDING_FILE):
                    send({'result': 'no_match'})
                    continue
                stored = np.load(EMBEDDING_FILE)
                img = load_image(req.get('image_path'))
                emb, score, face_count = get_embedding(img)
                h, w = img.shape[:2]
                log(f'[face_recog] recognize: {w}x{h} faces={face_count} score={score:.2f}')

                if emb is None:
                    send({'result': 'no_face'})
                    continue

                similarity = float(np.dot(stored, emb))
                log(f'[face_recog] similarity={similarity:.3f}')
                send({'result': 'match' if similarity >= THRESHOLD else 'no_match', 'similarity': similarity})
            except Exception as e:
                log(f'[face_recog] error: {e}')
                send({'result': 'error', 'msg': str(e)})

        else:
            send({'result': 'error', 'msg': 'unknown cmd'})


if __name__ == '__main__':
    main()
