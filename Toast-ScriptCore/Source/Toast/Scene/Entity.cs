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

        public string Name
        {
            get
            {
                if (HasComponent<TagComponent>())
                {
                    // Directly retrieve the Tag without using GetComponent<T>()
                    return InternalCalls.TagComponent_GetTag(ID);
                }
                else
                {
                    return "Unknown";
                }
            }
        }

        public bool HasComponent<T>() where T : Component, new()
        {
            Type componentType = typeof(T);
            return InternalCalls.Entity_HasComponent(ID, componentType);
        }

        public T GetComponent<T>() where T : Component, new() 
        {
            if (!HasComponent<T>())
            {
                string entityName = Name;
                string componentType = typeof(T).Name;

                Console.LogError("The Entity '{0}' doesn't have the component: '{1}'", entityName, componentType);

                return null;
            }

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

        public Entity FindChildEntityByName(string name, string childName)
        {
            ulong entityID = InternalCalls.Entity_FindChildEntityByName(name, childName);

            if (entityID == 0)
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
