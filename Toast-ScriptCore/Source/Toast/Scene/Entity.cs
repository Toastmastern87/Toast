using System;

namespace Toast 
{

    public class Entity
    {
        protected Entity() { ID = 0; }

        internal Entity(ulong id)
        {
            ID = id;
        }

        public readonly ulong ID;

        //public Vector3 Translation
        //{
        //    get
        //    {
        //        InternalCalls.Entity_GetTranslation(ID, out Vector3 result);
        //        return result;
        //    }

        //    set
        //    {
        //        InternalCalls.Entity_SetTranslation(ID, ref value);
        //    }
        //}

        public bool HasComponent<T>() where T : Component, new()
        {
            Type componentType = typeof(T);
            return InternalCalls.Entity_HasComponent(ID, componentType);
        }

        public T GetComponent<T>() where T : Component, new() 
        {
            if (!HasComponent<T>())
                return null;

            T component = new T() { Entity = this };
            return component;
        }

    }

}
