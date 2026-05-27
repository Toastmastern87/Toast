using Source.Toast.Math;
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
        public float ZoomFactor = 1.12f;
        public float ZoomSmoothTime = 0.18f;      // seconds to settle (0.12 snappy, 0.25 smoother)
        public float MinZoomStepMeters = 0.25f;   // prevents tiny jitter near ground
        public float MaxZoomStepMeters = 250000f; // safety clamp per frame (tune as desired)
        public float ZoomPercentNear = 0.10f;      // 10% per step near ground (try 0.06–0.12)
        public float ZoomPercentFar = 0.01f;      // 1% per step at high altitude (try 0.005–0.02)
        public float ZoomRampPower = 0.65f;      // how quickly it transitions (0.5–1.2)
        public float ZoomRampRefAlt = 20000.0f;   // altitude where it starts slowing down

        public float WheelSensitivity = 1.0f;     // 0.25–1.0 typical
        public float ReferenceAltitude = 1000.0f;

        public float BaseMovementSpeed = 500.0f;

        private float mZoomTargetAltitude = -1.0f;

        private TransformComponent mCameraTransformComponent;
        private CameraComponent mCameraComponent;

        private bool mIsFollowing = false;
        private TransformComponent mFollowTransform;

        private Vector3 mCameraWorldRightVector;
        private Vector3 mCameraWorldForwardVector;

        private Vector2 mCursorPos;

        private float altitude = 0.0f;

        // Following state
        private Quaternion mStartRotation;
        private Vector3 mStartTranslation;
        private Vector3 mPreviousTranslation;

        private float Clamp(float value, float min, float max)
        {
            return (value < min) ? min : (value > max) ? max : value;
        }

        void OnCreate()
        {
            mCameraTransformComponent = GetComponent<TransformComponent>();
            mCameraComponent = GetComponent<CameraComponent>();

            mZoomTargetAltitude = PhysicsEngine.GetAltitude(this.ID, true);
            mZoomTargetAltitude = Clamp(mZoomTargetAltitude, MinAltitude, MaxAltitude);
        }

        void OnEvent()
        {
        }

        void OnUpdate(float ts)
        {
            if (Input.IsKeyPressed(KeyCode.Escape) && mIsFollowing)
            {
                mIsFollowing = false;
                mCameraTransformComponent.Rotation = mStartRotation;
                mCameraComponent.WorldTranslation = mStartTranslation;
            }

            Vector3 upWorld = Vector3.Normalize(-1.0f * (Planet.Translation + mCameraComponent.WorldTranslation));
            //Vector3 upWorld = Vector3.Normalize(mCameraComponent.WorldTranslation - Planet.Translation);

            altitude = PhysicsEngine.GetAltitude(this.ID, true);

            Matrix4 cameraTransform = GetComponent<TransformComponent>().GetTransform();
            mCameraWorldRightVector = Vector3.Normalize(new Vector3(cameraTransform.D00, cameraTransform.D10, cameraTransform.D20));
            mCameraWorldForwardVector = Vector3.Normalize(new Vector3(cameraTransform.D02, cameraTransform.D12, cameraTransform.D22));

            ////////// CAMERA ROTATION ////////////////

            Vector2 newCursorPos = Input.GetMousePosition();

            if (Input.IsMouseButtonPressed(MouseCode.ButtonMiddle))
            {
                Vector2 delta = newCursorPos - mCursorPos;
                delta.Y = -delta.Y; // Invert Y axis

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

            if (!mIsFollowing)
            {
                float scrollDelta = Input.GetMouseWheelDelta();

                if (mZoomTargetAltitude < 0.0f)
                {
                    // Safety init if OnCreate didn't run as expected
                    mZoomTargetAltitude = Clamp(altitude, MinAltitude, MaxAltitude);
                }

                if (Math.Abs(scrollDelta) > 0.001f)
                {
                    // Some input systems report large deltas; apply sensitivity
                    float wheel = scrollDelta * WheelSensitivity;

                    // Convert wheel to an integer-ish number of "steps" while still supporting fractional deltas (track pads)
                    // Positive wheel usually means scroll up; you used -scrollDelta before, keep same behavior:
                    // wheel > 0 => zoom in (decrease altitude)
                    // wheel < 0 => zoom out (increase altitude)
                    float steps = wheel;

                    // Altitude-dependent percentage per wheel step.
                    // Near ground: ~ZoomPercentNear
                    // High altitude: ~ZoomPercentFar
                    float a = Clamp(altitude, MinAltitude, MaxAltitude);
                    float x = a / Math.Max(1.0f, ZoomRampRefAlt);           // normalized by reference altitude
                    float t = (float)Math.Pow(x / (1.0f + x), ZoomRampPower); // 0..1 smooth ramp
                    float zoomPercent = ZoomPercentNear + (ZoomPercentFar - ZoomPercentNear) * t; // lerp

                    // Convert percent -> multiplicative factor per step (1 + percent)
                    float perStepFactor = 1.0f + zoomPercent;

                    // Apply steps (supports fractional wheel deltas too):
                    // wheel > 0 => zoom in => reduce altitude => multiply by perStepFactor^(-steps)
                    float factor = (float)Math.Pow((double)perStepFactor, (double)(-steps));
                    mZoomTargetAltitude *= factor;

                    // Clamp target
                    mZoomTargetAltitude = Clamp(mZoomTargetAltitude, MinAltitude, MaxAltitude);
                }

                // Smoothly approach target altitude (exponential smoothing)
                float dt = ts / Scene.TimeScale;
                if (dt > 0.0f)
                {
                    // alpha = 1 - exp(-dt / tau)
                    float tau = Math.Max(0.0001f, ZoomSmoothTime);
                    float alpha = 1.0f - (float)Math.Exp(-(double)(dt / tau));

                    float prevAlt = altitude;

                    // Use your measured altitude as the current state (already updated earlier in OnUpdate)
                    float newAlt = prevAlt + (mZoomTargetAltitude - prevAlt) * alpha;

                    // Convert altitude change to world movement
                    float deltaAltitude = prevAlt - newAlt; // positive => move "down" along upWorld, matching your old sign convention

                    // Small dead zone to prevent micro jitter
                    if (Math.Abs(deltaAltitude) < MinZoomStepMeters)
                        deltaAltitude = 0.0f;

                    // Safety clamp per frame to avoid spikes
                    deltaAltitude = Clamp(deltaAltitude, -MaxZoomStepMeters, MaxZoomStepMeters);

                    if (Math.Abs(deltaAltitude) > 0.0f)
                        mCameraComponent.WorldTranslation += upWorld * deltaAltitude;
                }

                Input.SetMouseWheelDelta(0.0f);
            }

            ////////// WASD MOVEMENT ////////////////
            ///
            if (!mIsFollowing)
            {
                Vector3 keyboardDirection = Vector3.Zero;
                Vector3 up = new Vector3(0.0f, 1.0f, 0.0f);

                if (Input.IsKeyPressed(KeyCode.W))
                    keyboardDirection += mCameraWorldForwardVector;
                if (Input.IsKeyPressed(KeyCode.S))
                    keyboardDirection -= mCameraWorldForwardVector;
                if (Input.IsKeyPressed(KeyCode.D))
                    keyboardDirection += mCameraWorldRightVector;
                if (Input.IsKeyPressed(KeyCode.A))
                    keyboardDirection -= mCameraWorldRightVector;

                if (Vector3.LengthSquared(keyboardDirection) > 1e-6f)
                {
                    keyboardDirection = Vector3.Normalize(keyboardDirection);

                    float keyboardSpeed = BaseMovementSpeed * (1.0f + altitude / (ReferenceAltitude * 0.2f));
                    Vector3 moveWS = keyboardDirection * keyboardSpeed * (ts / Scene.TimeScale);

                    // Sub-step to prevent tunneling at high speed
                    float maxStep = 200.0f; // meters (tune)
                    float len = Vector3.Length(moveWS);
                    int steps = Math.Max(1, (int)Math.Ceiling(len / maxStep));
                    Vector3 stepMove = moveWS / steps;

                    const float eps = 0.25f;   // safety margin
                    const int pushIters = 5;

                    for (int s = 0; s < steps; s++)
                    {
                        // Move the world opposite the intended camera movement
                        mCameraComponent.WorldTranslation += -stepMove;

                        // Camera is at origin in floating origin space
                        float a = PhysicsEngine.GetAltitudeAtWorldPos(Vector3.Zero);

                        // Push-out if below minimum altitude
                        for (int i = 0; i < pushIters && a < MinAltitude; i++)
                        {
                            float delta = (MinAltitude - a) + eps;

                            // push world DOWN to move camera UP relative to ground
                            mCameraComponent.WorldTranslation += -up * delta;

                            a = PhysicsEngine.GetAltitudeAtWorldPos(Vector3.Zero);
                        }

                        // Optional max altitude clamp
                        if (MaxAltitude > MinAltitude)
                        {
                            float a2 = PhysicsEngine.GetAltitudeAtWorldPos(Vector3.Zero);
                            if (a2 > MaxAltitude)
                            {
                                float delta = (a2 - MaxAltitude) + eps;

                                // pull world UP to move camera DOWN relative to ground
                                mCameraComponent.WorldTranslation += up * delta;
                            }
                        }
                    }
                }
            }
            else 
            {
                // Following logic
                if (mFollowTransform != null)
                {
                    Vector3 followDelta = mFollowTransform.Translation - mPreviousTranslation;
                    mCameraComponent.WorldTranslation += -followDelta;

                    mPreviousTranslation = mFollowTransform.Translation;
                }
            }
        }

        public void StartFollowing(TransformComponent targetTransform)
        {
            mIsFollowing = true;
            mFollowTransform = targetTransform;

            mPreviousTranslation = mFollowTransform.Translation;
            mStartRotation = mCameraTransformComponent.Rotation;
            mStartTranslation = mCameraComponent.WorldTranslation;

            mCameraComponent.WorldTranslation = -(mFollowTransform.Translation + new Vector3(0.0f, 0.0f, -100.0f));
            mCameraTransformComponent.Rotation = Quaternion.Identity;
        }
    }
}