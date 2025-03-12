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

        private TransformComponent mMarsTransform;
        private TransformComponent mCameraTransformComponent;
        private CameraComponent mCameraComponent;
        private SphereColliderComponent mCollider;
        private RigidBodyComponent mRigidbody;
        private PlanetComponent mMarsPlanet;
        private Vector3 mCameraRightVector;
        private Vector3 mCameraForwardVector;

        private Matrix4 mCameraTransform;

        private Vector2 mCursorPos;

        private float minAltitudeForCamera = 10.0f;
        private float maxAltitudeForCamera = 60000.0f;
        private float minCameraFar = 250000.0f;
        private float maxCameraFar = 800000.0f;
        private float altitude = 0.0f;
        private float tCamera = 0.0f;

        private float Clamp(float value, float min, float max)
        {
            return (value < min) ? min : (value > max) ? max : value;
        }

        void OnCreate()
        {
            mCameraTransformComponent = GetComponent<TransformComponent>();
            mCameraComponent = GetComponent<CameraComponent>();
            mCollider = GetComponent<SphereColliderComponent>();
            mRigidbody = GetComponent<RigidBodyComponent>();

            mMarsTransform = FindEntityByName("Mars").GetComponent<TransformComponent>();
            mMarsPlanet = FindEntityByName("Mars").GetComponent<PlanetComponent>();

            //Entity starshipSN2 = FindEntityByName("Starship SN2");
            //if (starshipSN2 != null)
            //    mStarshipSN2Transform = starshipSN2.GetComponent<TransformComponent>();
            //else 
            //{
            //    mStarshipSN2Transform = null;
            //    Toast.Console.LogError("Starship SN2 entity not found.");
            //}

            //Entity starshipSN3 = FindEntityByName("Starship SN3");
            //if (starshipSN3 != null)
            //    mStarshipSN3Transform = starshipSN2.GetComponent<TransformComponent>();
            //else
            //{
            //    mStarshipSN3Transform = null;
            //    Toast.Console.LogError("'Starship SN3' entity not found.");
            //}

            tCamera = (altitude - minAltitudeForCamera) / (maxAltitudeForCamera - minAltitudeForCamera);
            tCamera = Clamp(tCamera, 0.0f, 1.0f);

            mCollider.ReqAltitude = true;
            Toast.Console.LogTrace("mCollider.ReqAltitude: " + mCollider.ReqAltitude);
            if (MaxAltitude < MinAltitude)
                MaxAltitude = MinAltitude;
        }

        void OnEvent()
        {
        }

        void OnUpdate(float ts)
        {
            float newCameraFar = minCameraFar + (maxCameraFar - minCameraFar) * tCamera;
            mCameraComponent.FarClip = newCameraFar;

            if (altitude > 1000.0f)
                mCollider.ReqAltitude = false;
            else
                mCollider.ReqAltitude = true;

            if (altitude <= 1000.0f && mCollider.ReqAltitude)
                altitude = mRigidbody.Altitude;
               

           // Toast.Console.LogTrace("Altitude at start of Update: " + altitude + ", mCollider.ReqAltitude: " + mCollider.ReqAltitude);
            mCameraTransform = GetComponent<TransformComponent>().GetTransform();
            mCameraRightVector = new Vector3(mCameraTransform.D00, mCameraTransform.D10, mCameraTransform.D20);
            mCameraForwardVector = new Vector3(mCameraTransform.D02, mCameraTransform.D12, mCameraTransform.D22);

            Vector2 newCursorPos = Input.GetMousePosition();

            if (Input.IsMouseButtonPressed(MouseCode.ButtonRight))
            {
                if (mCursorPos.X != newCursorPos.X)
                    mCameraTransformComponent.Yaw += (newCursorPos.X - mCursorPos.X) * (ts / Scene.TimeScale) * MouseSpeedFactor;

                if (mCursorPos.Y != newCursorPos.Y)
                    mCameraTransformComponent.Pitch -= (newCursorPos.Y - mCursorPos.Y) * (ts / Scene.TimeScale) * MouseSpeedFactor;
            }

            mCursorPos = newCursorPos;

            float scaleFactor = 5.5f; // or a value that suits your needs
            float zoomSpeed = BaseZoomSpeed * (1.0f + scaleFactor * (float)Math.Log(1.0f + altitude / ReferenceAltitude));
            zoomSpeed = Clamp(zoomSpeed, 1.0f, 15000.0f);

            float scrollDelta = Input.GetMouseWheelDelta();

            if (Math.Abs(scrollDelta) > 0.001f)
            {
                float newAltitude = 0.0f;
                float deltaAltitude = 0.0f;

                deltaAltitude = zoomSpeed * (ts / Scene.TimeScale) * -scrollDelta;

                newAltitude = Clamp(altitude + deltaAltitude, MinAltitude, MaxAltitude);
                deltaAltitude = altitude - newAltitude;
                altitude = newAltitude;

                if (Math.Abs(deltaAltitude) > 0.0001f || scrollDelta > 0.0f)
                {
                    Vector3 normalizedDirection = Vector3.Normalize(mMarsTransform.Translation);
                    //Toast.Console.LogCritical("zoomSpeed: " + zoomSpeed + ", ts: " + ts + ", Scene.TimeScale: " + Scene.TimeScale + ", scrollDelta: " + scrollDelta);
                    //Toast.Console.LogCritical("deltaAltitude: " + deltaAltitude + ", MinAltitude: " + MinAltitude + ", MaxAltitude: " + MaxAltitude);
                    mCameraComponent.AddWorldMovement(normalizedDirection * -deltaAltitude);
                }
            }

            Input.SetMouseWheelDelta(0.0f);

            // WASD Movement
            Vector3 keyboardDirection = Vector3.Zero;
            if (Input.IsKeyPressed(KeyCode.W))
            {
                keyboardDirection += mCameraForwardVector;
            }
            if (Input.IsKeyPressed(KeyCode.S))
            {
                keyboardDirection -= mCameraForwardVector;
            }
            if (Input.IsKeyPressed(KeyCode.D))
            {
                keyboardDirection += mCameraRightVector;
            }
            if (Input.IsKeyPressed(KeyCode.A))
            {
                keyboardDirection -= mCameraRightVector;
            }

            if (Vector3.Length(keyboardDirection) > 0.0f)
            {
                // Normalize to have consistent movement speed when moving diagonally.
                keyboardDirection = Vector3.Normalize(keyboardDirection);

                // Define a base speed. You can tweak this value to suit your needs.
                float baseMovementSpeed = 50.0f;

                // Increase the speed with altitude so that at higher altitudes the camera travels faster.
                float keyboardSpeed = baseMovementSpeed * (1.0f + altitude / (ReferenceAltitude * 0.2f));

                // Calculate the movement vector scaled by time
                Vector3 keyboardMovement = keyboardDirection * keyboardSpeed * (ts / Scene.TimeScale);
                keyboardDirection = -1.0f * keyboardMovement;

                // Call AddWorldMovement with the negative vector so the world moves opposite to the intended camera motion.
                mCameraComponent.AddWorldMovement(keyboardDirection);
            }
        }
    }
}