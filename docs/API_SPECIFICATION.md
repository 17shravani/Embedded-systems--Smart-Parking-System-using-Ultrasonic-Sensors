# 📡 Smart Parking REST API Specification

The embedded ESP32 web server exposes lightweight JSON endpoints designed for integration with Smart City centralized dashboards, mobile applications, and gate access kiosks.

---

## 1. Get Real-Time Parking Telemetry

- **Endpoint:** `/api/status`
- **Method:** `GET`
- **Response Format:** `application/json`

### Example Response:
```json
{
  "total": 4,
  "available": 2,
  "occupied": 2,
  "gateOpen": false,
  "slots": [
    { "id": 1, "occupied": true, "distance": 14.2 },
    { "id": 2, "occupied": false, "distance": 118.5 },
    { "id": 3, "occupied": true, "distance": 18.0 },
    { "id": 4, "occupied": false, "distance": 105.3 }
  ]
}
```

---

## 2. Remote Gate Barrier Trigger

- **Endpoint:** `/api/gate/open`
- **Method:** `POST`
- **Response Format:** `application/json`

### Success Response (Free Slots Available):
```json
{
  "status": "success",
  "message": "Gate opened for 3 seconds."
}
```

### Rejection Response (Parking Full):
```json
{
  "status": "rejected",
  "message": "Cannot open gate: Parking is FULL!"
}
```
