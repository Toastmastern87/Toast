using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Toast
{
    public static class Selection
    {
        public static void Clear()
        {
            InternalCalls.Selection_Clear();
        }

        public static Entity[] GetSelected()
        {
            uint count = InternalCalls.Selection_GetCount();
            Entity[] result = new Entity[count];
            for (uint i = 0; i < count; i++)
            {
                ulong id = InternalCalls.Selection_GetAt(i);
                result[i] = id == 0 ? null : new Entity(id);
            }
            return result;
        }
    }
}