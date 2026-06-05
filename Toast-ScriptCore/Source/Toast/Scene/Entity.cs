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

        public Entity FindParentEntity(ulong entityID)
        {
            ulong parentEntityID = InternalCalls.Entity_FindParentEntity(entityID);

            if (parentEntityID == 0)
                return null;

            return new Entity(parentEntityID);
        }

        public Entity FindDecententByName(ulong parentID, string childName)
        {
            ulong decententID = InternalCalls.Entity_FindDecententByName(parentID, childName);

            if (decententID == 0)
                return null;

            return new Entity(decententID);
        }

        public void Select()
        {
            InternalCalls.Entity_Select(ID);
        }

        public void Deselect()
        {
            InternalCalls.Entity_Deselect(ID);
        }

        public void SelectExclusive()
        {
            InternalCalls.Entity_SelectExclusive(ID);
        }

        public bool IsSelected()
        {
            return InternalCalls.Entity_IsSelected(ID);
        }

        public void MoveTo(Vector3 target, float speed)
        {
            InternalCalls.Entity_MoveTo(ID, (double)target.X, (double)target.Y, (double)target.Z, speed);
        }

        public bool IsSelectable()
        {
            return InternalCalls.Entity_IsSelectable(ID);
        }

        public T As<T>() where T : Entity, new() 
        {
            object instance = InternalCalls.Script_GetInstance(ID);
            return instance as T;
        }

        public void Unparent() 
        {
            InternalCalls.Entity_Unparent(ID);
        }

    }

}
