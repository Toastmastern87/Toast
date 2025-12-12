using System;
using System.IO;
using Toast;

namespace Sandbox
{
    public class CameraController : Entity
    {
        public float MinAltitude = 0.0f;
        public float MaxAltitude = 0.0f;
        public float MouseSpeedFactor = 1.0f;

        public float BaseZoomSpeed = 10.0f;
        public float ZoomFactor = 1.5f;
        public float ReferenceAltitude = 1000.0f;

        public float BaseMovementSpeed = 500.0f;

        private TransformComponent mCameraTransformComponent;
        private CameraComponent mCameraComponent;

        private Vector3 mCameraWorldRightVector;
        private Vector3 mCameraWorldForwardVector;

        private Vector2 mCursorPos;

        private float altitude = 0.0f;

        private float Clamp(float value, float min, float max)
        {
            return (value < min) ? min : (value > max) ? max : value;
        }

        void OnCreate()
        {
            mCameraTransformComponent = GetComponent<TransformComponent>();
            mCameraComponent = GetComponent<CameraComponent>();
        }

        void OnEvent()
        {
        }

        void OnUpdate(float ts)
        {
            Vector3 upWorld = Vector3.Normalize(-1.0f * (Planet.Translation + mCameraComponent.WorldTranslation));
            //Vector3 upWorld = Vector3.Normalize(mCameraComponent.WorldTranslation - Planet.Translation);

            altitude = PhysicsEngine.GetAltitude(this.ID, true);

            Matrix4 cameraTransform = GetComponent<TransformComponent>().GetTransform();
            mCameraWorldRightVector = Vector3.Normalize(new Vector3(cameraTransform.D00, cameraTransform.D10, cameraTransform.D20));
            mCameraWorldForwardVector = Vector3.Normalize(new Vector3(cameraTransform.D02, cameraTransform.D12, cameraTransform.D22));

            ////////// CAMERA ROTATION ////////////////

            Vector2 newCursorPos = Input.GetMousePosition();

            if (Input.IsMouseButtonPressed(MouseCode.ButtonRight))
            {
                Vector2 delta = newCursorPos - mCursorPos;

                if (Vector3.LengthSquared(upWorld) < 1e-6f) 
                    upWorld = new Vector3(0.0f, 1.0f, 0.0f);

                float yawDeg = delta.X * MouseSpeedFactor;   // ← no dt
                float pitchDeg = -delta.Y * MouseSpeedFactor;

                if (Math.Abs(yawDeg) > 0.0001f)
                    mCameraTransformComponent.TransformComponent_Rotate(upWorld, yawDeg);

                Matrix4 M = mCameraTransformComponent.GetTransform();
                Vector3 fwdWorld = Vector3.Normalize(new Vector3(M.D02, M.D12, M.D22));
                Vector3 rightWorld = Vector3.Normalize(new Vector3(M.D00, M.D10, M.D20));

                // 4) Build leveled forward (project onto horizon plane ⟂ upWorld)
                float fwdDotUp = Vector3.Dot(fwdWorld, upWorld);
                Vector3 fwdT = fwdWorld - upWorld * fwdDotUp;
                float fwdTLen2 = Vector3.LengthSquared(fwdT);

                // Robust pole fallback: if forward ≈ up, project RIGHT instead
                if (fwdTLen2 < 1e-6f)
                {
                    Vector3 rightT = rightWorld - upWorld * Vector3.Dot(rightWorld, upWorld);
                    if (Vector3.LengthSquared(rightT) >= 1e-6f)
                    {
                        rightWorld = Vector3.Normalize(rightT);
                        fwdT = Vector3.Normalize(Vector3.Cross(rightWorld, upWorld)); // LH: fwd = right × up
                    }
                    else
                    {
                        pitchDeg = 0.0f; // completely degenerate this frame
                        fwdT = new Vector3(0, 0, 1);
                    }
                }
                else
                {
                    fwdT = Vector3.Normalize(fwdT);
                    rightWorld = Vector3.Normalize(Vector3.Cross(upWorld, fwdT));     // LH: right = up × fwd
                }

                // 5) Pitch around horizon right (world axis)
                if (Math.Abs(pitchDeg) > 0.0001f)
                    mCameraTransformComponent.TransformComponent_Rotate(rightWorld, pitchDeg);

                M = mCameraTransformComponent.GetTransform();
                Vector3 upCam = Vector3.Normalize(new Vector3(M.D01, M.D11, M.D21));
                fwdWorld = Vector3.Normalize(new Vector3(M.D02, M.D12, M.D22));

                // Project ups onto plane ⟂ forward
                Vector3 upCamProj = Vector3.Normalize(upCam - fwdWorld * Vector3.Dot(upCam, fwdWorld));
                Vector3 upWorldProj = Vector3.Normalize(upWorld - fwdWorld * Vector3.Dot(upWorld, fwdWorld));

                float sinRoll = Vector3.Dot(Vector3.Cross(upCamProj, upWorldProj), fwdWorld);
                float cosRoll = Vector3.Dot(upCamProj, upWorldProj);
                float rollDeg = (float)Math.Atan2((double)sinRoll, (double)cosRoll) * (180.0f / (float)Math.PI);

                if (Math.Abs(rollDeg) > 0.01f)
                    mCameraTransformComponent.TransformComponent_Rotate(fwdWorld, rollDeg);
            }

            mCursorPos = newCursorPos;

            ////////// CAMERA ZOOMING ////////////////

            float scrollDelta = Input.GetMouseWheelDelta();

            if (Math.Abs(scrollDelta) > 0.001f)
            {
                float scaleFactor = 5.5f; // or a value that suits your needs
                float zoomSpeed = BaseZoomSpeed * (1.0f + scaleFactor * (float)Math.Log(1.0f + altitude / ReferenceAltitude));
                zoomSpeed = Clamp(zoomSpeed, 1.0f, 15000.0f);

                float newAltitude = 0.0f;
                float deltaAltitude = 0.0f;
                deltaAltitude = zoomSpeed * (ts / Scene.TimeScale) * -scrollDelta;

                newAltitude = Clamp(altitude + deltaAltitude, MinAltitude, MaxAltitude);
                deltaAltitude = altitude - newAltitude;

                if (Math.Abs(deltaAltitude) > 0.0001f || scrollDelta > 0.0f)
                    mCameraComponent.AddWorldMovement(upWorld * deltaAltitude);
            }

            Input.SetMouseWheelDelta(0.0f);

            //////////// WASD MOVEMENT ////////////////
            //Vector3 keyboardDirection = Vector3.Zero;

            //if (Input.IsKeyPressed(KeyCode.W))
            //    keyboardDirection += mCameraWorldForwardVector;
            //if (Input.IsKeyPressed(KeyCode.S))
            //    keyboardDirection -= mCameraWorldForwardVector;
            //if (Input.IsKeyPressed(KeyCode.D))
            //    keyboardDirection += mCameraWorldRightVector;
            //if (Input.IsKeyPressed(KeyCode.A))
            //    keyboardDirection -= mCameraWorldRightVector;

            //if (Vector3.Length(keyboardDirection) > 0.0f)
            //{
            //    keyboardDirection = Vector3.Normalize(keyboardDirection);

            //    float keyboardSpeed = BaseMovementSpeed * (1.0f + altitude / (ReferenceAltitude * 0.2f));

            //    // This is the real world-space movement
            //    Vector3 cameraMoveWS = keyboardDirection * keyboardSpeed * (ts / Scene.TimeScale);

            //    // Current camera world position
            //    Vector3 camWS = -mCameraComponent.WorldTranslation;

            //    // Predicted world position
            //    Vector3 newCamWS = camWS + cameraMoveWS;

            //    // Predict new altitude
            //    float predictedAltitude = PhysicsEngine.GetAltitudeAtWorldPos(newCamWS);

            //    // Clamp based on min/max altitudes
            //    if (predictedAltitude < MinAltitude || predictedAltitude > MaxAltitude)
            //    {
            //        // Stop movement (or clamp it to edge if you prefer)
            //        return;
            //    }

            //    // Apply movement normally (invert so world shifts opposite)
            //    mCameraComponent.AddWorldMovement(-cameraMoveWS);
            //}

            ////////// WASD MOVEMENT ////////////////
            Vector3 keyboardDirection = Vector3.Zero;

            if (Input.IsKeyPressed(KeyCode.W))
                keyboardDirection += mCameraWorldForwardVector;
            if (Input.IsKeyPressed(KeyCode.S))
                keyboardDirection -= mCameraWorldForwardVector;
            if (Input.IsKeyPressed(KeyCode.D))
                keyboardDirection += mCameraWorldRightVector;
            if (Input.IsKeyPressed(KeyCode.A))
                keyboardDirection -= mCameraWorldRightVector;

            if (Vector3.Length(keyboardDirection) > 0.0f)
            {
                keyboardDirection = Vector3.Normalize(keyboardDirection);

                float keyboardSpeed = BaseMovementSpeed * (1.0f + altitude / (ReferenceAltitude * 0.2f));

                // This is the real world-space movement
                Vector3 cameraMoveWS = keyboardDirection * keyboardSpeed * (ts / Scene.TimeScale);

                // Current camera world position
                Vector3 camWS = -mCameraComponent.WorldTranslation;

                // First, do the move
                Vector3 newCamWS = camWS + cameraMoveWS;

                // Query altitude at the new position
                float newAltitude = PhysicsEngine.GetAltitudeAtWorldPos(newCamWS);

                // ----- CLAMP MIN ALTITUDE BY PUSHING UP -----
                if (newAltitude < MinAltitude)
                {
                    // How much we need to move *up* to get back to MinAltitude
                    float delta = MinAltitude - newAltitude;

                    // upWorld should be your radial/tangent "up" at the camera
                    // (normalized, pointing away from planet)
                    newCamWS += upWorld * delta;
                }

                // ----- CLAMP MAX ALTITUDE (optional, same idea) -----
                if (newAltitude > MaxAltitude)
                {
                    float delta = MaxAltitude - newAltitude; // negative
                    newCamWS += upWorld * delta;
                }

                // Recompute the final movement from original position
                cameraMoveWS = newCamWS - camWS;

                // Apply movement (invert so world shifts opposite)
                mCameraComponent.AddWorldMovement(-cameraMoveWS);
            }
        }
    }
}