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

        public Entity FindEntityByName(string name) 
        {
            ulong entityID = InternalCalls.Entity_FindEntityByName(name);

            if(entityID == 0)
                return null;

            return new Entity(entityID); 
        }

        public T As<T>() where T : Entity, new() 
        {
            object instance = InternalCalls.Script_GetInstance(ID);
            return instance as T;
        }

    }

}
