#include <vulkan/vulkan.h>

#include <iostream>
#include <stdexcept>
#include <glm/glm.hpp>

#include "Engine.h"

/* Commands:
*
*   - addSkybox(
*        fileName,
*        folderName
*     );
*   - addObjectPBR(
*        name,
*        folderName,
*        fileName,
*        position,
*        rotation,
*        size
*     );
*   - addDirectionalLight(
*        name,
*        folderName,
*        fileName,
*        color,
*        position,
*        targetPosition,
*        size
*     );
*   - addSpotLight(
*        name,
*        folderName,
*        fileName,
*        color,
*        position,
*        targetPosition,
*        rotation,
*        size
*     );
*   - addPointLight(
*        name,
*        folderName,
*        fileName,
*        color,
*        position,
*        size
*     );
*/

int main()
{
    Engine  app;

    try
    {
        app.run();
    }

    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return 0;
    }
    return 0;
}