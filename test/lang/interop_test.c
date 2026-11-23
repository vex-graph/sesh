/** Public header composition only: R5 may include both Graphvex and Sesh. */

#include "vex.h"
#include "graphvex/api/graphvex.h"
#include "lang/sesh.h"
#include <assert.h>

i32 main()
{
    GraphicsRect rect = GraphicsRect_0();
    SeshClient sessionClient = SeshClient(1);
    assert(GraphicsRect_isEmpty(rect) && SeshClient_getId(&sessionClient) == 1);
    return 0;
}
