# qw_color_manager_v1 Usage Guide

## Overview

The `qw_color_manager_v1` wrapper provides Qt-style bindings for wlroots' color management protocol v1 implementation. This protocol allows Wayland compositors to manage color spaces and image descriptions for surfaces.

## Version Requirements

**Important**: The color management v1 protocol is only available in wlroots 0.19.0 and later. The wrapper includes version guards to ensure compatibility:

```cpp
#if WLR_VERSION_MINOR >= 19
// Color management code here
#endif
```

## Available Classes

### qw_color_manager_v1

Main color manager class that handles the color management protocol.

**Static Methods:**
- `create(wl_display *display, uint32_t version, const wlr_color_manager_v1_options *options)` - Create a new color manager instance

**Member Methods:**
- `set_surface_preferred_image_description(wlr_surface *surface, const wlr_image_description_v1_data *data)` - Set preferred image description for a surface

### qw_image_description_v1_data

Utility class for working with image description data.

**Static Methods:**
- `get(wlr_surface *surface)` - Get image description data from a surface

## Basic Usage Example

```cpp
#include "types/qwcolormanagementv1.h"

// Check version availability
#if WLR_VERSION_MINOR >= 19

void setupColorManagement(wl_display *display) {
    // Configure color manager features
    wlr_color_manager_v1_options options = {};
    options.features.icc_v2_v4 = true;
    options.features.parametric = true;
    // ... configure other features as needed

    // Create color manager
    auto *colorManager = qw_color_manager_v1::create(display, 1, &options);
    if (!colorManager) {
        qWarning("Failed to create color manager");
        return;
    }

    // The color manager is now ready to handle color management requests
}

void setSurfaceColorSpace(wlr_surface *surface) {
    // Get current image description
    const auto *imageDesc = qw_image_description_v1_data::get(surface);
    if (imageDesc) {
        // Process image description data
        qDebug("Surface has image description with transfer function: %u",
               imageDesc->tf_named);
    }
}

#endif // WLR_VERSION_MINOR >= 19
```

## Integration with Existing Code

The wrapper follows qwlroots patterns and integrates seamlessly with other Qt-style wlroots bindings:

```cpp
// In your compositor setup
void MyCompositor::initializeColorManagement() {
#if WLR_VERSION_MINOR >= 19
    wlr_color_manager_v1_options options = {};
    // Configure supported features based on your hardware/renderer capabilities
    options.features.icc_v2_v4 = hasICCSupport();
    options.features.parametric = hasParametricSupport();

    m_colorManager = qw_color_manager_v1::create(m_display->handle(), 1, &options);
    if (m_colorManager) {
        qDebug("Color management v1 protocol enabled");
    }
#else
    qDebug("Color management v1 requires wlroots >= 0.19");
#endif
}
```

## Error Handling

Always check for version compatibility and handle creation failures:

```cpp
#if WLR_VERSION_MINOR >= 19
    auto *manager = qw_color_manager_v1::create(display, version, &options);
    if (!manager) {
        // Handle creation failure
        qWarning("Failed to create color manager - check options and display");
        return false;
    }
#else
    qWarning("Color management v1 not available in this wlroots version");
    return false;
#endif
```

## Testing

The wrapper includes comprehensive unit tests that automatically skip when the required wlroots version is not available:

```bash
# Run color management tests
ctest -R QWColorManagementV1

# All tests
ctest
```

## See Also

- [wlroots color management documentation](https://gitlab.freedesktop.org/wlroots/wlroots/-/blob/master/include/wlr/types/wlr_color_management_v1.h)
- [Wayland color management protocol specification](https://gitlab.freedesktop.org/wayland/wayland-protocols/-/blob/main/staging/color-management/color-management-v1.xml)
- [qwlroots wrapping patterns guide](./qwlroots-wrapping-patterns-best-practices-en.md)
