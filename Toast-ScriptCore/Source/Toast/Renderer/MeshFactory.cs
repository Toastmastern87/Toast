using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Runtime.CompilerServices;

namespace Toast
{
    public static class MeshFactory
    {

        public static Mesh CreatePlanet()
        {
            return new Mesh(CreatePlanet_Native());
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern IntPtr CreatePlanet_Native();
    }

}
