using System;
using Toast;

namespace Sandbox
{
    public class CameraController : Entity
    {
        public float MinAltitude = 0.0f;
        public float MaxAltitude = 0.0f;
        public float ZoomM = 0.0f;
        public float ZoomK = 0.0f;
        public float MoveM = 0.0f;
        public float MoveK = 0.0f;
        public float MouseSpeedFactor = 1.0f;

        private TransformComponent mMarsTransform;
        private TransformComponent mCameraTransformComponent;
        private TransformComponent mStarshipTransform;
        private PlanetComponent mMarsPlanet;
        private Vector3 mCameraRightVector;
        private Vector3 mCameraForwardVector;

        private Matrix4 mCameraTransform;

        private Vector2 mCursorPos;

        void OnCreate()
        {
            mCameraTransformComponent = GetComponent<TransformComponent>();

            mMarsTransform = FindEntityByName("Mars").GetComponent<TransformComponent>();
            mMarsPlanet = FindEntityByName("Mars").GetComponent<PlanetComponent>();

            mStarshipTransform = FindEntityByName("Starship Atmosphere").GetComponent<TransformComponent>();

            if (MaxAltitude < MinAltitude)
                MaxAltitude = MinAltitude;
        }

        void OnEvent()
        {
        }

        void OnUpdate(float ts)
        {
            //Toast.Console.LogCritical("Inside OnUpdate");
            float altitude = Vector3.Length(mMarsTransform.Translation) - mMarsPlanet.Radius;

            float zoomSpeed = (ZoomK * (float)Math.Pow(altitude, 2.0) + ZoomM);
            float moveSpeed = (MoveK * (float)Math.Pow(altitude, 2.0) + MoveM);

            mCameraTransform = GetComponent<TransformComponent>().GetTransform();
            mCameraRightVector = new Vector3(mCameraTransform.D00, mCameraTransform.D10, mCameraTransform.D20);
            mCameraForwardVector = new Vector3(mCameraTransform.D02, mCameraTransform.D12, mCameraTransform.D22);

            Vector2 newCursorPos = Input.GetMousePosition();

            if (Input.IsMouseButtonPressed(MouseCode.ButtonRight))
            {
                if (mCursorPos.X != newCursorPos.X)
                    mCameraTransformComponent.Yaw += (newCursorPos.X - mCursorPos.X) * (ts / Scene.TimeScale) * MouseSpeedFactor;

                if (mCursorPos.Y != newCursorPos.Y)
                    mCameraTransformComponent.Pitch += (newCursorPos.Y - mCursorPos.Y) * (ts / Scene.TimeScale) * MouseSpeedFactor;
            }

            mCursorPos = newCursorPos;

            //Toast.Console.LogCritical("Pitch and yaw done");

            if (Input.GetMouseWheelDelta() != 0.0f)
            {
                float deltaAltitude = zoomSpeed * (ts / Scene.TimeScale) * -Input.GetMouseWheelDelta();

                // Makes sure that the new altitude is within the limits
                float newAltitude = altitude + deltaAltitude;
                newAltitude = Math.Min(newAltitude, MaxAltitude);
                newAltitude = Math.Max(newAltitude, MinAltitude);

                deltaAltitude = altitude - newAltitude;

                // Check that the values of delta altitude is not to small
                deltaAltitude = deltaAltitude <= 0.0001f && Input.GetMouseWheelDelta() > 0.0f ? 0.0f : deltaAltitude;

                mMarsTransform.Translation -= Vector3.Normalize(mMarsTransform.Translation) * deltaAltitude;
                mStarshipTransform.Translation -= (Vector3.Normalize(mMarsTransform.Translation - mStarshipTransform.Translation)) * deltaAltitude;
            }

            if (Input.IsKeyPressed(KeyCode.W))
            {
                mMarsTransform.TransformComponent_Rotate(mCameraRightVector, (-moveSpeed * ts));
                mStarshipTransform.TransformComponent_RotateAroundPoint(mMarsTransform.Translation, mCameraRightVector, (-moveSpeed * (ts / Scene.TimeScale)));
            }


            if (Input.IsKeyPressed(KeyCode.S))
            {
                mMarsTransform.TransformComponent_Rotate(mCameraRightVector, (moveSpeed * ts));
                mStarshipTransform.TransformComponent_RotateAroundPoint(mMarsTransform.Translation, mCameraRightVector, (moveSpeed * (ts / Scene.TimeScale)));
            }

            if (Input.IsKeyPressed(KeyCode.A))
            {
                mMarsTransform.TransformComponent_Rotate(mCameraForwardVector, (-moveSpeed * ts));
                mStarshipTransform.TransformComponent_RotateAroundPoint(mMarsTransform.Translation, mCameraForwardVector, (-moveSpeed * (ts / Scene.TimeScale)));
            }

            if (Input.IsKeyPressed(KeyCode.D))
            {
                mMarsTransform.TransformComponent_Rotate(mCameraForwardVector, (moveSpeed * ts));
                mStarshipTransform.TransformComponent_RotateAroundPoint(mMarsTransform.Translation, mCameraForwardVector, (moveSpeed * (ts / Scene.TimeScale)));
            }

            Input.SetMouseWheelDelta(0.0f);

            //Toast.Console.LogCritical("Done with OnUpdate");
        }
    }
}