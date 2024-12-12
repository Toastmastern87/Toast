using System;
using System.Runtime.InteropServices;

namespace Toast
{
    public abstract class Component
    {
        public Entity Entity { get; internal set; }
    }

    public class TagComponent : Component
    {
        public string Tag
        {
            get => InternalCalls.TagComponent_GetTag(Entity.ID);
            set => InternalCalls.TagComponent_SetTag(Entity.ID, value);
        }
    }

    public class TransformComponent : Component
    {
        public Vector3 Translation
        {
            get
            {
                InternalCalls.TransformComponent_GetTranslation(Entity.ID, out Vector3 result);
                return result;
            }

            set
            {
                InternalCalls.TransformComponent_SetTranslation(Entity.ID, ref value);
            }
        }
        public Vector3 Rotation
        {
            get
            {
                InternalCalls.TransformComponent_GetRotation(Entity.ID, out Vector3 result);
                return result;
            }

            set
            {
                InternalCalls.TransformComponent_SetRotation(Entity.ID, ref value);
            }
        }
        public float Pitch
        {
            get
            {
                InternalCalls.TransformComponent_GetPitch(Entity.ID, out float result);
                return result;
            }

            set
            {
                InternalCalls.TransformComponent_SetPitch(Entity.ID, ref value);
            }
        }
        public float Yaw
        {
            get
            {
                InternalCalls.TransformComponent_GetYaw(Entity.ID, out float result);
                return result;
            }

            set
            {
                InternalCalls.TransformComponent_SetYaw(Entity.ID, ref value);
            }
        }
        public float Roll
        {
            get
            {
                InternalCalls.TransformComponent_GetRoll(Entity.ID, out float result);
                return result;
            }

            set
            {
                InternalCalls.TransformComponent_SetRoll(Entity.ID, ref value);
            }
        }
        public Vector3 Scale
        {
            get
            {
                InternalCalls.TransformComponent_GetScale(Entity.ID, out Vector3 result);
                return result;
            }

            set
            {
                InternalCalls.TransformComponent_SetScale(Entity.ID, ref value);
            }
        }

        public Matrix4 GetTransform()
        {
            InternalCalls.TransformComponent_GetTransform(Entity.ID, out Matrix4 result);
            return result;
        }

        public void TransformComponent_Rotate(Vector3 rotationAxis, float angle)
        {
            InternalCalls.TransformComponent_Rotate(Entity.ID, ref rotationAxis, ref angle);
        }

        public void TransformComponent_RotateAroundPoint(Vector3 point, Vector3 rotationAxis, float angle)
        {
            InternalCalls.TransformComponent_RotateAroundPoint(Entity.ID, ref point, ref rotationAxis, ref angle);
        }
    }

    public class CameraComponent : Component
    {
        public Camera Camera
        {
            get
            {
                Camera result = new Camera(InternalCalls.CameraComponent_GetCamera(Entity.ID));
                return result;
            }

            set
            {
                IntPtr ptr = value == null ? IntPtr.Zero : value.mUnmanagedInstance;
                InternalCalls.CameraComponent_SetCamera(Entity.ID, ptr);
            }
        }
        public float FarClip
        {
            get
            {
                return InternalCalls.CameraComponent_GetFarClip(Entity.ID);
            }
            set
            {
                InternalCalls.CameraComponent_SetFarClip(Entity.ID, value);
            }
        }
        public float NearClip
        {
            get
            {
                return InternalCalls.CameraComponent_GetNearClip(Entity.ID);
            }
            set
            {
                InternalCalls.CameraComponent_SetNearClip(Entity.ID, value);
            }
        }
    }

    public class PlanetComponent : Component
    {
        public Mesh Mesh
        {
            get
            {
                Mesh result = new Mesh(InternalCalls.PlanetComponent_GetMesh(Entity.ID));
                return result;
            }

            set
            {
                IntPtr ptr = value == null ? IntPtr.Zero : value.mUnmanagedInstance;
                InternalCalls.PlanetComponent_SetMesh(Entity.ID, ptr);
            }
        }

        public float Radius
        {
            get
            {
                InternalCalls.PlanetComponent_GetRadius(Entity.ID, out float result);
                return result;
            }
            set
            {
            }
        }

        public int Subdivisions
        {
            get
            {
                InternalCalls.PlanetComponent_GetSubdivisions(Entity.ID, out int result);
                return result;
            }
            set
            {
            }
        }

        public double[] DistanceLUT
        {
            get
            {
                return InternalCalls.PlanetComponent_GetDistanceLUT(Entity.ID);
            }
            set
            {
            }
        }

        public double[] FaceLevelDotLUT
        {
            get
            {
                return InternalCalls.PlanetComponent_GetFaceLevelDotLUT(Entity.ID);
            }
            set
            {
            }
        }
        public void RegeneratePlanet(Vector3 cameraPos, Matrix4 cameraTransform)
        {
            InternalCalls.PlanetComponent_GeneratePlanet(Entity.ID, cameraPos, cameraTransform);
        }
    }

    public class MeshComponent : Component
    {
        public Mesh Mesh
        {
            get
            {
                Mesh result = new Mesh(InternalCalls.MeshComponent_GetMesh(Entity.ID));
                return result;
            }

            set
            {
                IntPtr ptr = value == null ? IntPtr.Zero : value.mUnmanagedInstance;
                InternalCalls.MeshComponent_SetMesh(Entity.ID, ptr);
            }
        }

        public void PlayAnimation(string name, float startTime)
        {
            InternalCalls.MeshComponent_PlayAnimation(Entity.ID, name, startTime);
        }

        public float StopAnimation(string name)
        {
            return InternalCalls.MeshComponent_StopAnimation(Entity.ID, name);
        }

        public float GetDurationAnimation(string name)
        {
            return InternalCalls.MeshComponent_GetDurationAnimation(Entity.ID, name);
        }

        public void RegeneratePlanet(Vector3 cameraPos, Matrix4 cameraTransform)
        {
            InternalCalls.MeshComponent_RegeneratePlanet(Entity.ID, cameraPos, cameraTransform);
        }
    }

    public class UIPanelComponent : Component
    {
        public bool Visible
        {
            get
            {
                return InternalCalls.UIPanelComponent_GetVisible(Entity.ID);
            }

            set
            {
                InternalCalls.UIPanelComponent_SetVisible(Entity.ID, value);
            }
        }

        public float BorderSize
        {
            get
            {
                return InternalCalls.UIPanelComponent_GetBorderSize(Entity.ID);
            }

            set
            {

            }
        }
    }

    public class UIButtonComponent : Component
    {
        public Vector4 Color
        {
            get
            {
                InternalCalls.UIButtonComponent_GetColor(Entity.ID, out Vector4 result);
                return result;
            }

            set
            {
                InternalCalls.UIButtonComponent_SetColor(Entity.ID, ref value);
            }
        }
    }

    public class UITextComponent : Component
    {
        public string Text
        {
            get => InternalCalls.UITextComponent_GetText(Entity.ID);
            set => InternalCalls.UITextComponent_SetText(Entity.ID, value);
        }
    }

    public class RigidBodyComponent : Component
    {
        public float Altitude
        {
            get
            {
                return InternalCalls.RigidBodyComponent_GetAltitude(Entity.ID);
            }
            set
            {
            }
        }

        public Vector3 LinearVelocity
        {
            get
            {
                InternalCalls.RigidBodyComponent_GetLinearVelocity(Entity.ID, out Vector3 result);
                return result;
            }
            set
            {
            }
        }
    }
}
