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
        private TransformComponent mStarshipSN2Transform;
        private TransformComponent mStarshipSN3Transform;
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

            Entity starshipSN2 = FindEntityByName("Starship SN2");
            if (starshipSN2 != null)
                mStarshipSN2Transform = starshipSN2.GetComponent<TransformComponent>();
            else 
            {
                mStarshipSN2Transform = null;
                Toast.Console.LogError("Starship SN2 entity not found.");
            }

            Entity starshipSN3 = FindEntityByName("Starship SN3");
            if (starshipSN3 != null)
                mStarshipSN3Transform = starshipSN2.GetComponent<TransformComponent>();
            else
            {
                mStarshipSN3Transform = null;
                Toast.Console.LogError("'Starship SN3' entity not found.");
            }

            if (MaxAltitude < MinAltitude)
                MaxAltitude = MinAltitude;
        }

        void OnEvent()
        {
        }

        void OnUpdate(float ts)
        {
            float altitude = Vector3.Length(mMarsTransform.Translation) - mMarsPlanet.Radius;

            //Toast.Console.LogCritical("altitude: " + altitude);
            //Toast.Console.LogCritical("mCameraTransformComponent: " + mCameraTransformComponent.Translation.X + ", " + mCameraTransformComponent.Translation.Y + ", " + mCameraTransformComponent.Translation.Z);

            float zoomSpeed = (ZoomK * (float)Math.Pow(altitude, 2.0) + ZoomM);
            float moveSpeed = (MoveK/100000.0f * (float)Math.Pow(altitude, 2.0) + MoveM);

            //Toast.Console.LogCritical("moveSpeed: " + moveSpeed);

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
                if (mStarshipSN2Transform != null)
                    mStarshipSN2Transform.Translation -= (Vector3.Normalize(mMarsTransform.Translation - mStarshipSN2Transform.Translation)) * deltaAltitude;
                if (mStarshipSN3Transform != null)
                    mStarshipSN3Transform.Translation -= (Vector3.Normalize(mMarsTransform.Translation - mStarshipSN3Transform.Translation)) * deltaAltitude;
            }

            if (Input.IsKeyPressed(KeyCode.W))
            {
                mMarsTransform.TransformComponent_Rotate(mCameraRightVector, (-moveSpeed * ts / Scene.TimeScale));
                if (mStarshipSN2Transform != null)
                    mStarshipSN2Transform.TransformComponent_RotateAroundPoint(mMarsTransform.Translation, mCameraRightVector, (-moveSpeed * (ts / Scene.TimeScale)));
                if (mStarshipSN3Transform != null)
                    mStarshipSN3Transform.TransformComponent_RotateAroundPoint(mMarsTransform.Translation, mCameraRightVector, (-moveSpeed * (ts / Scene.TimeScale)));
            }


            if (Input.IsKeyPressed(KeyCode.S))
            {
                mMarsTransform.TransformComponent_Rotate(mCameraRightVector, (moveSpeed * ts / Scene.TimeScale));
                if (mStarshipSN2Transform != null)
                    mStarshipSN2Transform.TransformComponent_RotateAroundPoint(mMarsTransform.Translation, mCameraRightVector, (moveSpeed * (ts / Scene.TimeScale)));
                if (mStarshipSN3Transform != null)
                    mStarshipSN3Transform.TransformComponent_RotateAroundPoint(mMarsTransform.Translation, mCameraRightVector, (moveSpeed * (ts / Scene.TimeScale)));
            }

            if (Input.IsKeyPressed(KeyCode.A))
            {
                mMarsTransform.TransformComponent_Rotate(mCameraForwardVector, (-moveSpeed * ts / Scene.TimeScale));
                if (mStarshipSN2Transform != null)
                    mStarshipSN2Transform.TransformComponent_RotateAroundPoint(mMarsTransform.Translation, mCameraForwardVector, (-moveSpeed * (ts / Scene.TimeScale)));
                if (mStarshipSN3Transform != null)
                    mStarshipSN3Transform.TransformComponent_RotateAroundPoint(mMarsTransform.Translation, mCameraForwardVector, (-moveSpeed * (ts / Scene.TimeScale)));
            }

            if (Input.IsKeyPressed(KeyCode.D))
            {
                mMarsTransform.TransformComponent_Rotate(mCameraForwardVector, (moveSpeed * ts / Scene.TimeScale));
                if (mStarshipSN2Transform != null)
                    mStarshipSN2Transform.TransformComponent_RotateAroundPoint(mMarsTransform.Translation, mCameraForwardVector, (moveSpeed * (ts / Scene.TimeScale)));
                if (mStarshipSN3Transform != null)
                    mStarshipSN3Transform.TransformComponent_RotateAroundPoint(mMarsTransform.Translation, mCameraForwardVector, (moveSpeed * (ts / Scene.TimeScale)));
            }

            Input.SetMouseWheelDelta(0.0f);

            //Toast.Console.LogCritical("Done with OnUpdate");
        }
    }
}