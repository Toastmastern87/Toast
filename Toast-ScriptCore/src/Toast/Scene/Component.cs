using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Runtime.CompilerServices;

namespace Toast
{
    public abstract class Component
    {
        public Entity Entity { get; set; }
    }

    public class TagComponent : Component
    {
        public string Tag
        {
            get => GetTag_Native(Entity.ID);
            set => SetTag_Native(Entity.ID, value);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern string GetTag_Native(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void SetTag_Native(ulong entityID, string tag);
    }

    public class TransformComponent : Component
    {
        public Matrix4 Transform
        {
            get
            {
                Matrix4 result;
                GetTransform_Native(Entity.ID, out result);
                return result;
            }
            set
            {
                SetTransform_Native(Entity.ID, ref value);
            }
        }

        public Vector3 Translation
        {
            get
            {
                GetTranslation_Native(Entity.ID, out Vector3 result);
                return result;
            }

            set
            {
                SetTranslation_Native(Entity.ID, ref value);
            }
        }
        public Vector3 Rotation
        {
            get
            {
                GetRotation_Native(Entity.ID, out Vector3 result);
                return result;
            }

            set
            {
                SetRotation_Native(Entity.ID, ref value);
            }
        }
        public Vector3 Scale
        {
            get
            {
                GetScale_Native(Entity.ID, out Vector3 result);
                return result;
            }

            set
            {
                SetScale_Native(Entity.ID, ref value);
            }
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetTransform_Native(ulong entityID, out Matrix4 outTranslation);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetTransform_Native(ulong entityID, ref Matrix4 inTranslation);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetTranslation_Native(ulong entityID, out Vector3 outTranslation);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetTranslation_Native(ulong entityID, ref Vector3 inTranslation);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void GetRotation_Native(ulong entityID, out Vector3 outRotation);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void SetRotation_Native(ulong entityID, ref Vector3 inRotation);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void GetScale_Native(ulong entityID, out Vector3 outScale);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void SetScale_Native(ulong entityID, ref Vector3 inScale);
    }
    public class CameraComponent : Component
    {
        public Camera Camera
        {
            get
            {
                Camera result = new Camera(GetCamera_Native(Entity.ID));
                return result;
            }

            set
            {
                IntPtr ptr = value == null ? IntPtr.Zero : value.mUnmanagedInstance;
                SetCamera_Native(Entity.ID, ptr);
            }
        }
        public float FarClip
        {
            get
            {
                return GetFarClip_Native(Entity.ID);
            }
            set
            {
                SetFarClip_Native(Entity.ID, value);
            }
        }
        public float NearClip
        {
            get
            {
                return GetNearClip_Native(Entity.ID);
            }
            set
            {
                SetNearClip_Native(Entity.ID, value);
            }
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern IntPtr GetCamera_Native(ulong entityID);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetCamera_Native(ulong entityID, IntPtr unmanagedInstance);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetFarClip_Native(ulong entityID, float farClip);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetFarClip_Native(ulong entityID);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetNearClip_Native(ulong entityID, float nearClip);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetNearClip_Native(ulong entityID);
    }
    public class PlanetComponent : Component
    {
        public Mesh Mesh
        {
            get
            {
                Mesh result = new Mesh(GetMesh_Native(Entity.ID));
                return result;
            }

            set
            {
                IntPtr ptr = value == null ? IntPtr.Zero : value.mUnmanagedInstance;
                SetMesh_Native(Entity.ID, ptr);
            }
        }

        public float Radius
        {
            get
            {
                GetRadius_Native(Entity.ID, out float result);
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
                GetSubdivisions_Native(Entity.ID, out int result);
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
                return GetDistanceLUT_Native(Entity.ID);
            }
            set
            {
            }
        }

        public double[] FaceLevelDotLUT
        {
            get
            {
                return GetFaceLevelDotLUT_Native(Entity.ID);
            }
            set
            {
            }
        }
        public void RegeneratePlanet(Vector3 cameraPos, Matrix4 cameraTransform)
        {
            RegeneratePlanet_Native(Entity.ID, cameraPos, cameraTransform);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern IntPtr GetMesh_Native(ulong entityID);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetMesh_Native(ulong entityID, IntPtr unmanagedInstance);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void RegeneratePlanet_Native(ulong entityID, Vector3 cameraPos, Matrix4 cameraForward);
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void GetRadius_Native(ulong entityID, out float inScale);
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void GetSubdivisions_Native(ulong entityID, out int inScale);
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern double[] GetDistanceLUT_Native(ulong entityID);
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern double[] GetFaceLevelDotLUT_Native(ulong entityID);
    }

    public class MeshComponent : Component
    {
        public Mesh Mesh
        {
            get
            {
                Mesh result = new Mesh(GetMesh_Native(Entity.ID));
                return result;
            }

            set
            {
                IntPtr ptr = value == null ? IntPtr.Zero : value.mUnmanagedInstance;
                SetMesh_Native(Entity.ID, ptr);
            }
        }

        public void RegeneratePlanet(Vector3 cameraPos, Matrix4 cameraTransform)
        {
            RegeneratePlanet_Native(Entity.ID, cameraPos, cameraTransform);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern IntPtr GetMesh_Native(ulong entityID);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetMesh_Native(ulong entityID, IntPtr unmanagedInstance);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void RegeneratePlanet_Native(ulong entityID, Vector3 cameraPos, Matrix4 cameraForward);

    }

    public class UIButtonComponent : Component
    {
        public Vector4 Color
        {
            get
            {
                GetColor_Native(Entity.ID, out Vector4 result);
                return result;
            }

            set
            {
                SetColor_Native(Entity.ID, ref value);
            }
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern IntPtr GetColor_Native(ulong entityID, out Vector4 result);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetColor_Native(ulong entityID, ref Vector4 inColor);
    }

    public class UITextComponent : Component
    {
        public string Text
        {
            get => GetText_Native(Entity.ID);
            set => SetText_Native(Entity.ID, value);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern string GetText_Native(ulong entityID);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetText_Native(ulong entityID, string text);
    }
}
