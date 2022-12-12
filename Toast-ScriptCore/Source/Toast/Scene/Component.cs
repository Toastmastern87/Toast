using System;

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
            InternalCalls.PlanetComponent_RegeneratePlanet(Entity.ID, cameraPos, cameraTransform);
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

        public void RegeneratePlanet(Vector3 cameraPos, Matrix4 cameraTransform)
        {
            InternalCalls.MeshComponent_RegeneratePlanet(Entity.ID, cameraPos, cameraTransform);
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
}
