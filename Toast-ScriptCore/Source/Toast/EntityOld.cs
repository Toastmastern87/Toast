using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;

namespace Toast
{
    public class EntityOld
    {
        public ulong ID { get; private set; }

        protected EntityOld() { ID = 0; }
        internal EntityOld(ulong id) { ID = id; }
        ~EntityOld() { }

        public T CreateComponent<T>() where T : Component, new()
        {
            CreateComponent_Native(ID, typeof(T));
            T component = new T();
            component.Entity = this;
            return component;
        }

        public bool HasComponent<T>() where T : Component, new()
        {
            return HasComponent_Native(ID, typeof(T));
        }

        public T GetComponent<T>() where T : Component, new()
        {
            if (HasComponent<T>()) 
            {
                T component = new T();
                component.Entity = this;
                return component;
            }
            return null;
        }

        public EntityOld FindEntityByTag(string tag) 
        {
            ulong entityID = FindEntityByTag_Native(tag);
            return new EntityOld(entityID);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void CreateComponent_Native(ulong entityID, Type type);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool HasComponent_Native(ulong entityID, Type type);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern ulong FindEntityByTag_Native(string tag);

    }

}
