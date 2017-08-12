#pragma once
/*
    sokol_gfx.h -- simple 3D API wrapper

    Do this:
        #define SOKOL_IMPL
    before you include this file in *one* C or C++ file to create the 
    implementation.

    In the same place define one of the following to select the rendering
    backend:
        #define SOKOL_GLCORE33
        #define SOKOL_GLES2
        #define SOKOL_GLES3
        #define SOKOL_D3D11
        #define SOKOL_METAL

    I.e. for the GL 3.3 Core Profile it should look like this:

    #include ...
    #include ...
    #define SOKOL_IMPL
    #define SOKOL_GLCORE33
    #include "sokol_gfx.h"

    Optionally provide the following defines with your own implementations:

    SOKOL_ASSERT(c)     - your own assert macro (default: assert(c))
    SOKOL_MALLOC(s)     - your own malloc function (default: malloc(s))
    SOKOL_FREE(p)       - your own free function (default: free(p))
    SOKOL_LOG(msg)      - your own logging function (default: puts(msg))
    SOKOL_UNREACHABLE() - a guard macro for unreachable code (default: assert(false))

    Optionally define the following to force debug checks and validations
    even in release mode:

    SOKOL_DEBUG         - by default this is defined if _DEBUG is defined
*/
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 
    Resource id typedefs:

    sg_buffer:      vertex- and index-buffers
    sg_image:       textures and render targets
    sg_shader:      vertex- and fragment-shaders, uniform blocks
    sg_pipeline:    associated shader and vertex-layouts, and render states
    sg_pass:        a bundle of render targets and actions on them

    Instead of pointers, resource creation functions return a 32-bit
    number which uniquely identifies the resource object.

    The 32-bit resource id is split into a 16-bit pool index in the lower bits, 
    and a 16-bit 'unique counter' in the upper bits. The index allows fast
    pool lookups, and combined with the unique-mask it allows to detect
    'dangling accesses' (trying to use an object which no longer exists, and
    its pool slot has been reused for a new object)

    The resource ids are wrapped into a struct so that the compiler
    can complain when the wrong resource type is used.
*/
typedef struct { uint32_t id; } sg_buffer;
typedef struct { uint32_t id; } sg_image;
typedef struct { uint32_t id; } sg_shader;
typedef struct { uint32_t id; } sg_pipeline;
typedef struct { uint32_t id; } sg_pass;

/* 
    various compile-time constants

    FIXME: it may make sense to convert some of those into defines so
    that the user code can override them.
*/
enum {
    SG_INVALID_ID = 0,
    SG_NUM_SHADER_STAGES = 2,
    SG_MAX_COLOR_ATTACHMENTS = 4,
    SG_MAX_SHADERSTAGE_BUFFERS = 4,
    SG_MAX_SHADERSTAGE_IMAGES = 12,
    SG_MAX_SHADERSTAGE_UBS = 4,
    SG_MAX_UB_MEMBERS = 16,
    SG_MAX_VERTEX_ATTRIBUTES = 16,
    SG_MAX_MIPMAPS = 16,
};

/*
    sg_feature

    These are optional features, use the function
    sg_query_feature() to check whether the feature is supported.
*/
typedef enum {
    SG_FEATURE_INSTANCED_ARRAYS,
    SG_FEATURE_TEXTURE_COMPRESSION_DXT,
    SG_FEATURE_TEXTURE_COMPRESSION_PVRTC,
    SG_FEATURE_TEXTURE_COMPRESSION_ATC,
    SG_FEATURE_TEXTURE_COMPRESSION_ETC2,
    SG_FEATURE_TEXTURE_FLOAT,
    SG_FEATURE_TEXTURE_HALF_FLOAT,
    SG_FEATURE_ORIGIN_BOTTOM_LEFT,
    SG_FEATURE_ORIGIN_TOP_LEFT,
    SG_FEATURE_MSAA_RENDER_TARGETS,
    SG_FEATURE_PACKED_VERTEX_FORMAT_10_2,
    SG_FEATURE_MULTIPLE_RENDER_TARGET,
    SG_FEATURE_IMAGETYPE_3D,
    SG_FEATURE_IMAGETYPE_ARRAY,

    SG_NUM_FEATURES
} sg_feature;

/*
    sg_resource_state

    The current state of a resource in its resource pool.
    Resources start in the INITIAL state, which means the 
    pool slot is unoccupied and can be allocated. When a resource is
    created, first an id is allocated, and the resource pool slot
    is set to state ALLOC. After allocation, the resource is
    initialized, which may result in the VALID or FAILED state. The
    reason why allocation and initialization are separate is because
    some resource types (e.g. buffers and images) might be asynchronously
    initialized by the user application. If a resource which is not
    in the VALID state is attempted to be used for rendering, rendering
    operations will silently be dropped.
*/
typedef enum {
    SG_RESOURCESTATE_INITIAL,
    SG_RESOURCESTATE_ALLOC,
    SG_RESOURCESTATE_VALID,
    SG_RESOURCESTATE_FAILED,
} sg_resource_state;

/*
    sg_usage

    A resource usage hint describing the update strategy of
    buffers and images. This is used in the sg_buffer_desc.usage
    and sg_image_desc.usage members when creating buffers
    and images:

    SG_USAGE_IMMUTABLE:     the resource will never be updated with
                            new data, instead the data content of the
                            resource must be provided on creation
    SG_USAGE_DYNAMIC:       the resource will be updated infrequently
                            with new data (this could range from "once
                            after creation", to "quite often but not
                            every frame")
    SG_USAGE_STREAM:        the resource will be updated each frame
                            with new content

    The rendering backends use this hint to prevent that the
    CPU needs to wait for the GPU when attempting to update
    a resource that might be currently accessed by the GPU.

    Resource content is updated with the function sg_update_buffer() for
    buffer objects, and sg_update_image() for image objects. Only
    one update is allowed per frame and resource object. The
    application must update all data required for rendering (this
    means that the update data can be smaller than the resource size,
    if only a part of the overall resource size is used for rendering,
    you only need to make sure that the data that *is* used is valid.

    The default usage is SG_USAGE_IMMUTABLE.
*/
typedef enum {
    _SG_USAGE_DEFAULT,      /* value 0 reserved for default-init */
    SG_USAGE_IMMUTABLE,
    SG_USAGE_DYNAMIC,
    SG_USAGE_STREAM,
    _SG_USAGE_NUM,
} sg_usage;

/*
    sg_buffer_type

    This indicates whether a buffer contains vertex- or index-data,
    used in the sg_buffer_desc.type member when creating a buffer.

    The default value is SG_BUFFERTYPE_VERTEXBUFFER.
*/
typedef enum {
    _SG_BUFFERTYPE_DEFAULT,         /* value 0 reserved for default-init */
    SG_BUFFERTYPE_VERTEXBUFFER,
    SG_BUFFERTYPE_INDEXBUFFER,
    _SG_BUFFERTYPE_NUM,
} sg_buffer_type;

/*
    sg_index_type

    Indicates whether indexed rendering (fetching vertex-indices from an
    index buffer) is used, and if yes, the index data type (16- or 32-bits).
    This is used in the sg_pipeline_desc.index_type member when creating a
    pipeline object.

    The default index type is SG_INDEXTYPE_NONE.
*/
typedef enum {
    _SG_INDEXTYPE_DEFAULT,   /* value 0 reserved for default-init */
    SG_INDEXTYPE_NONE,
    SG_INDEXTYPE_UINT16,
    SG_INDEXTYPE_UINT32,
    _SG_INDEXTYPE_NUM,
} sg_index_type;

/*
    sg_image_type

    Indicates the basic image type (2D-texture, cubemap, 3D-texture
    or 2D-array-texture). 3D- and array-textures are not supported
    on the GLES2/WebGL backend. The image type is used in the
    sg_image_desc.type member when creating an image.

    The default image type when creating an image is SG_IMAGETYPE_2D.
*/
typedef enum {
    _SG_IMAGETYPE_DEFAULT,  /* value 0 reserved for default-init */
    SG_IMAGETYPE_2D,
    SG_IMAGETYPE_CUBE,
    SG_IMAGETYPE_3D,
    SG_IMAGETYPE_ARRAY,
    _SG_IMAGETYPE_NUM,
} sg_image_type;

/*
    sg_shader_stage

    There are 2 shader stages: vertex- and fragment-shader-stage.
    Each shader stage consists of:

    - one slot for a shader function (provided as source- or byte-code)
    - SG_MAX_SHADERSTAGE_UBS slots for uniform blocks
    - SG_MAX_SHADERSTAGE_IMAGES slots for images used as textures by
      the shader function
*/
typedef enum {
    SG_SHADERSTAGE_VS,
    SG_SHADERSTAGE_FS,
} sg_shader_stage;

/*
    sg_pixel_format

    This is a common subset of useful and widely supported pixel formats. The
    pixel format enum is mainly used when creating an image object in the
    sg_image_desc.pixel_format member.

    The default pixel format when creating an image is SG_PIXELFORMAT_RGBA8.
*/
typedef enum {
    _SG_PIXELFORMAT_DEFAULT,    /* value 0 reserved for default-init */
    SG_PIXELFORMAT_NONE,
    SG_PIXELFORMAT_RGBA8,
    SG_PIXELFORMAT_RGB8,
    SG_PIXELFORMAT_RGBA4,
    SG_PIXELFORMAT_R5G6B5,
    SG_PIXELFORMAT_R5G5B5A1,
    SG_PIXELFORMAT_R10G10B10A2,
    SG_PIXELFORMAT_RGBA32F,
    SG_PIXELFORMAT_RGBA16F,
    SG_PIXELFORMAT_R32F,
    SG_PIXELFORMAT_R16F,
    SG_PIXELFORMAT_L8,
    SG_PIXELFORMAT_DXT1,
    SG_PIXELFORMAT_DXT3,
    SG_PIXELFORMAT_DXT5,
    SG_PIXELFORMAT_DEPTH,
    SG_PIXELFORMAT_DEPTHSTENCIL,
    SG_PIXELFORMAT_PVRTC2_RGB,
    SG_PIXELFORMAT_PVRTC4_RGB,
    SG_PIXELFORMAT_PVRTC2_RGBA,
    SG_PIXELFORMAT_PVRTC4_RGBA,
    SG_PIXELFORMAT_ETC2_RGB8,
    SG_PIXELFORMAT_ETC2_SRGB8,
    _SG_PIXELFORMAT_NUM,
} sg_pixel_format;

/*
    sg_primitive_type

    This is the common subset of 3D primitive types supported across all 3D
    APIs. This is used in the sg_pipeline_desc.primitive_type member when
    creating a pipeline object.

    The default primitive type is SG_PRIMITIVETYPE_TRIANGLES.
*/
typedef enum {
    _SG_PRIMITIVETYPE_DEFAULT,  /* value 0 reserved for default-init */
    SG_PRIMITIVETYPE_POINTS,
    SG_PRIMITIVETYPE_LINES,
    SG_PRIMITIVETYPE_LINE_STRIP,
    SG_PRIMITIVETYPE_TRIANGLES,
    SG_PRIMITIVETYPE_TRIANGLE_STRIP,
    _SG_PRIMITIVETYPE_NUM,
} sg_primitive_type;

/*
    sg_filter

    The filtering mode when sampling a texture image. This is
    used in the sg_image_desc.min_filter and sg_image_desc.mag_filter
    members when creating an image object.

    The default filter mode is SG_FILTER_NEAREST.
*/
typedef enum {
    _SG_FILTER_DEFAULT, /* value 0 reserved for default-init */
    SG_FILTER_NEAREST,
    SG_FILTER_LINEAR,
    SG_FILTER_NEAREST_MIPMAP_NEAREST,
    SG_FILTER_NEAREST_MIPMAP_LINEAR,
    SG_FILTER_LINEAR_MIPMAP_NEAREST,
    SG_FILTER_LINEAR_MIPMAP_LINEAR,
    _SG_FILTER_NUM,
} sg_filter;

/*
    sg_wrap

    The texture coordinates wrapping mode when sampling a texture
    image. This is used in the sg_image_desc.wrap_u, .wrap_v
    and .wrap_w members when creating an image.

    The default wrap mode is SG_WRAP_REPEAT.
*/
typedef enum {
    _SG_WRAP_DEFAULT,   /* value 0 reserved for default-init */
    SG_WRAP_REPEAT,
    SG_WRAP_CLAMP_TO_EDGE,
    SG_WRAP_MIRRORED_REPEAT,
    _SG_WRAP_NUM,
} sg_wrap;

/*
    sg_vertex_format

    The data type of a vertex component. This is used to describe
    the layout of vertex data when creating a pipeline object.
*/
typedef enum {
    SG_VERTEXFORMAT_INVALID,
    SG_VERTEXFORMAT_FLOAT,
    SG_VERTEXFORMAT_FLOAT2,
    SG_VERTEXFORMAT_FLOAT3,
    SG_VERTEXFORMAT_FLOAT4,
    SG_VERTEXFORMAT_BYTE4,
    SG_VERTEXFORMAT_BYTE4N,
    SG_VERTEXFORMAT_UBYTE4,
    SG_VERTEXFORMAT_UBYTE4N,
    SG_VERTEXFORMAT_SHORT2,
    SG_VERTEXFORMAT_SHORT2N,
    SG_VERTEXFORMAT_SHORT4,
    SG_VERTEXFORMAT_SHORT4N,
    SG_VERTEXFORMAT_UINT10_N2,
    _SG_VERTEXFORMAT_NUM,
} sg_vertex_format;

/*
    sg_vertex_step

    Defines whether the input pointer of a vertex input stream is advanced
    'per vertex' or 'per instance'. The default step-func is
    SG_VERTEXSTEP_PER_VERTEX. SG_VERTEXSTEP_PER_INSTANCE is used with
    instanced-rendering.

    The vertex-step is part of the vertex-layout definition
    when creating pipeline objects.
*/
typedef enum {
    _SG_VERTEXSTEP_DEFAULT,     /* value 0 reserved for default-init */
    SG_VERTEXSTEP_PER_VERTEX,
    SG_VERTEXSTEP_PER_INSTANCE,
    _SG_VERTEXSTEP_NUM,
} sg_vertex_step;

/*
    sg_uniform_type

    The data type of a uniform block member. This is used to 
    describe the internal layout of uniform blocks when creating
    a shader object.
*/
typedef enum {
    SG_UNIFORMTYPE_INVALID,
    SG_UNIFORMTYPE_FLOAT,
    SG_UNIFORMTYPE_FLOAT2,
    SG_UNIFORMTYPE_FLOAT3,
    SG_UNIFORMTYPE_FLOAT4,
    SG_UNIFORMTYPE_MAT4,
    _SG_UNIFORMTYPE_NUM,
} sg_uniform_type;

/*
    sg_cull_mode

    The face-culling mode, this is used in the
    sg_pipeline_desc.rasterizer.cull_mode member when creating a 
    pipeline object.

    The default cull mode is SG_CULLMODE_NONE
*/
typedef enum {
    _SG_CULLMODE_DEFAULT,   /* value 0 reserved for default-init */
    SG_CULLMODE_NONE,
    SG_CULLMODE_FRONT,
    SG_CULLMODE_BACK,
    _SG_CULLMODE_NUM,
} sg_cull_mode;

/*
    sg_face_winding

    The vertex-winding rule that determines a front-facing primitive. This
    is used in the member sg_pipeline_desc.rasterizer.face_winding
    when creating a pipeline object.

    The default winding is SG_FACEWINDING_CW (clockwise)
*/
typedef enum {
    _SG_FACEWINDING_DEFAULT,    /* value 0 reserved for default-init */
    SG_FACEWINDING_CCW,
    SG_FACEWINDING_CW,
    _SG_FACEWINDING_NUM,
} sg_face_winding;

/*
    sg_compare_func

    The compare-function for depth- and stencil-ref tests.
    This is used when creating pipeline objects in the members:
   
    sg_pipeline_desc
        .depth_stencil
            .depth_compare_func
            .stencil_front.compare_func
            .stencil_back.compare_func

    The default compare func for depth- and stencil-tests is
    SG_COMPAREFUNC_ALWAYS.
*/
typedef enum {
    _SG_COMPAREFUNC_DEFAULT,    /* value 0 reserved for default-init */
    SG_COMPAREFUNC_NEVER,
    SG_COMPAREFUNC_LESS,
    SG_COMPAREFUNC_EQUAL,
    SG_COMPAREFUNC_LESS_EQUAL,
    SG_COMPAREFUNC_GREATER,
    SG_COMPAREFUNC_NOT_EQUAL,
    SG_COMPAREFUNC_GREATER_EQUAL,
    SG_COMPAREFUNC_ALWAYS,
    _SG_COMPAREFUNC_NUM,
} sg_compare_func;

/*
    sg_stencil_op

    The operation performed on a currently stored stencil-value when a
    comparison test passes or fails. This is used when creating a pipeline
    object in the members:

    sg_pipeline_desc
        .depth_stencil
            .stencil_front
                .fail_op
                .depth_fail_op
                .pass_op
            .stencil_back
                .fail_op
                .depth_fail_op
                .pass_op

    The default value is SG_STENCILOP_KEEP.
*/
typedef enum {
    _SG_STENCILOP_DEFAULT,      /* value 0 reserved for default-init */
    SG_STENCILOP_KEEP,
    SG_STENCILOP_ZERO,
    SG_STENCILOP_REPLACE,
    SG_STENCILOP_INCR_CLAMP,
    SG_STENCILOP_DECR_CLAMP,
    SG_STENCILOP_INVERT,
    SG_STENCILOP_INCR_WRAP,
    SG_STENCILOP_DECR_WRAP,
    _SG_STENCILOP_NUM,
} sg_stencil_op;

/*
    sg_blend_factor

    The source and destination factors in blending operations.
    This is used in the following members when creating a pipeline object:

    sg_pipeline_desc
        .blend
            .src_factor_rgb
            .dst_factor_rgb
            .src_factor_alpha
            .dst_factor_alpha

    The default value is SG_BLENDFACTOR_ONE for source
    factors, and SG_BLENDFACTOR_ZERO for destination factors.
*/
typedef enum {
    _SG_BLENDFACTOR_DEFAULT,    /* value 0 reserved for default-init */
    SG_BLENDFACTOR_ZERO,
    SG_BLENDFACTOR_ONE,
    SG_BLENDFACTOR_SRC_COLOR,
    SG_BLENDFACTOR_ONE_MINUS_SRC_COLOR,
    SG_BLENDFACTOR_SRC_ALPHA,
    SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
    SG_BLENDFACTOR_DST_COLOR,
    SG_BLENDFACTOR_ONE_MINUS_DST_COLOR,
    SG_BLENDFACTOR_DST_ALPHA,
    SG_BLENDFACTOR_ONE_MINUS_DST_ALPHA,
    SG_BLENDFACTOR_SRC_ALPHA_SATURATED,
    SG_BLENDFACTOR_BLEND_COLOR,
    SG_BLENDFACTOR_ONE_MINUS_BLEND_COLOR,
    SG_BLENDFACTOR_BLEND_ALPHA,
    SG_BLENDFACTOR_ONE_MINUS_BLEND_ALPHA,
    _SG_BLENDFACTOR_NUM,
} sg_blend_factor;

/*
    sg_blend_op

    Describes how the source and destination values are combined in the
    fragment blending operation. It is used in the following members when
    creating a pipeline object:

    sg_pipeline_desc
        .blend
            .op_rgb
            .op_alpha

    The default value is SG_BLENDOP_ADD.
*/
typedef enum {
    _SG_BLENDOP_DEFAULT,    /* value 0 reserved for default-init */
    SG_BLENDOP_ADD,
    SG_BLENDOP_SUBTRACT,
    SG_BLENDOP_REVERSE_SUBTRACT,
    _SG_BLENDOP_NUM,
} sg_blend_op;

/*
    sg_color_mask

    Selects the color channels when writing a fragment color to the
    framebuffer. This is used in the members
    sg_pipeline_desc.blend.color_write_mask when creating a pipeline object.

    The default colormask is SG_COLORMASK_RGBA (write all colors channels)
*/
typedef enum {
    _SG_COLORMASK_DEFAULT = 0,      /* value 0 reserved for default-init */
    SG_COLORMASK_NONE = (0x10),     /* special value for 'all channels disabled */
    SG_COLORMASK_R = (1<<0),
    SG_COLORMASK_G = (1<<1),
    SG_COLORMASK_B = (1<<2),
    SG_COLORMASK_A = (1<<3),
    SG_COLORMASK_RGB = 0x7,
    SG_COLORMASK_RGBA = 0xF,
} sg_color_mask;

/*
    sg_action

    Defines what action should be performed at the start of a render pass:

    SG_ACTION_CLEAR:    clear the render target image
    SG_ACTION_LOAD:     load the previous content of the render target image
    SG_ACTION_DONTCARE: leave the render target image content undefined

    This is used in the sg_pass_action structure. 
    
    The default action for all pass attachments is SG_ACTION_CLEAR, with the
    clear color rgba = {0.5f, 0.5f, 0.5f, 1.0f], depth=1.0 and stencil=0.

    If you want to override the default behaviour, it is important to not
    only set the clear color, but the 'action' field as well (as long as this
    is in its _SG_ACTION_DEFAULT, the value fields will be ignored).
*/
typedef enum {
    _SG_ACTION_DEFAULT,
    SG_ACTION_CLEAR,
    SG_ACTION_LOAD,
    SG_ACTION_DONTCARE,
    _SG_ACTION_NUM
} sg_action;

/*
    sg_pass_action

    The sg_pass_action struct defines the actions to be performed
    at the start of a rendering pass in the functions sg_begin_pass()
    and sg_begin_default_pass().

    A separate action and clear values can be defined for each
    color attachment, and for the depth-stencil attachment.

    The default clear values are defined by the macros:

    - SG_DEFAULT_CLEAR_RED:     0.5f
    - SG_DEFAULT_CLEAR_GREEN:   0.5f
    - SG_DEFAULT_CLEAR_BLUE:    0.5f
    - SG_DEFAULT_CLEAR_ALPHA:   1.0f
    - SG_DEFAULT_CLEAR_DEPTH:   1.0f
    - SG_DEFAULT_CLEAR_STENCIL: 0
*/
typedef struct {
    sg_action action;
    float val[4];
} sg_color_attachment_action;

typedef struct {
    sg_action action;
    float val;
} sg_depth_attachment_action;

typedef struct {
    sg_action action;
    uint8_t val;
} sg_stencil_attachment_action;

typedef struct {
    uint32_t _start_canary;
    sg_color_attachment_action colors[SG_MAX_COLOR_ATTACHMENTS];
    sg_depth_attachment_action depth;
    sg_stencil_attachment_action stencil;
    uint32_t _end_canary;
} sg_pass_action;

/*
    sg_draw_state

    The sg_draw_state structure defines the resource binding slots
    of the sokol_gfx render pipeline, used as argument to the
    sg_apply_draw_state() function.

    A draw state contains:

    - 1 pipeline object
    - 1..N vertex buffers
    - 0..1 index buffers
    - 0..N vertex shader stage images
    - 0..N fragment shader stage images

    The max number of vertex buffer and shader stage images
    are defined by the SG_MAX_SHADERSTAGE_BUFFERS and
    SG_MAX_SHADERSTAGE_IMAGES configuration constants.
*/
typedef struct {
    uint32_t _start_canary;
    sg_pipeline pipeline;
    sg_buffer vertex_buffers[SG_MAX_SHADERSTAGE_BUFFERS];
    sg_buffer index_buffer;
    sg_image vs_images[SG_MAX_SHADERSTAGE_IMAGES];
    sg_image fs_images[SG_MAX_SHADERSTAGE_IMAGES];
    uint32_t _end_canary;
} sg_draw_state;

/*
    sg_desc

    The sg_desc struct contains configuration values for sokol_gfx,
    it is used as parameter to the sg_setup() call.

    The default configuration is:

    .buffer_pool_size:      128
    .image_pool_size:       128
    .shader_pool_size:      32
    .pipeline_pool_size:    64
    .pass_pool_size:        16 
    
    Metal specific:
    .mtl_device     
        a pointer to the MTLDevice object, obtained with CFBridgingRetain()
    .mtl_renderpass_descriptor_cb
        a C callback function to obtain the MTLRenderPassDescriptor for the
        current frame when rendering to the default framebuffer, will be called 
        in sg_begin_default_pass(), the pointer must be obtained with
        CFBridgingRetain()
    .mtl_drawable_cb
        a C callback function to obtain a CAMetalDrawable for the current
        frame when rendering to the default framebuffer, will be called in
        sg_end_pass() of the default pass. the pointer must be obtained
        with CFBridgingRetain()
*/
typedef struct {
    uint32_t _start_canary;
    int buffer_pool_size;
    int image_pool_size;
    int shader_pool_size;
    int pipeline_pool_size;
    int pass_pool_size;
    /* Metal specific */
    const void* mtl_device;
    const void* (*mtl_renderpass_descriptor_cb)(void);
    const void* (*mtl_drawable_cb)(void);
    int mtl_global_uniform_buffer_size;
    uint32_t _end_canary;
} sg_desc;

/*
    sg_buffer_desc

    Creation parameters for sg_buffer objects, used in the
    sg_make_buffer() call.

    The default configuration is:

    .size:      0       (this *must* be set to a valid size in bytes)
    .type:      SG_BUFFERTYPE_VERTEXBUFFER
    .usage:     SG_USAGE_IMMUTABLE
    .data_ptr   0
    .data_size  0

    Buffers with the SG_USAGE_IMMUTABLE usage *must* fill the buffer
    with initial data (.data_ptr and .data_size cannot be 0).
*/
typedef struct {
    uint32_t _start_canary;
    int size;
    sg_buffer_type type;
    sg_usage usage;
    const void* data_ptr;
    int data_size;
    uint32_t _end_canary;
} sg_buffer_desc;

/*
    sg_image_desc

    Creation parameters for sg_image objects, used in the 
    sg_make_image() call.

    The default configuration is:

    .type:              SG_IMAGETYPE_2D
    .render_target:     false
    .width              0 (must be set to >0)
    .height             0 (must be set to >0)
    .depth/.layers:     1
    .num_mipmaps:       1
    .usage:             SG_USAGE_IMMUTABLE
    .pixel_format:      SG_PIXELFORMAT_RGBA8
    .sample_count:      1 (only used in render_targets)
    .min_filter:        SG_FILTER_NEAREST
    .mag_filter:        SG_FILTER_NEAREST
    .wrap_u:            SG_WRAP_REPEAT
    .wrap_v:            SG_WRAP_REPEAT
    .wrap_w:            SG_WRAP_REPEAT (only SG_IMAGETYPE_3D)
    .num_data_items:    0
    .data_ptrs:         0
    .data_sizes:        0

    SG_IMAGETYPE_ARRAY and SG_IMAGETYPE_3D are not supported on
    WebGL/GLES2, use sg_query_feature(SG_FEATURE_IMAGETYPE_ARRAY) and
    sg_query_feature(SG_FEATURE_IMAGETYPE_3D) at runtime to check
    if array- and 3D-textures are supported.

    Images with usage SG_USAGE_IMMUTABLE must be initialized with
    data using the num_data_items, data_ptrs and data_sizes members.

    The member data_ptrs is a pointer to an array of pointers, one per
    subimage to be filled with data. Subimages are cubemap-faces, mipmaps,
    array-layers and depth-slices.

    The member data_sizes is a pointer to an array of ints, with the
    size in bytes of each data item.

    Finally, num_data_items is the number of elements in each of the
    pointer- and size-arrays.

    TODO: describe the correct order of data items when initializing
    cubemaps, array- and 3D-textures and their mipmaps!
*/
typedef struct {
    uint32_t _start_canary;
    sg_image_type type;
    bool render_target;
    uint16_t width;
    uint16_t height;
    union {
        uint16_t depth;
        uint16_t layers;
    };
    uint16_t num_mipmaps;
    sg_usage usage;
    sg_pixel_format pixel_format;
    int sample_count;
    sg_filter min_filter;
    sg_filter mag_filter;
    sg_wrap wrap_u;
    sg_wrap wrap_v;
    sg_wrap wrap_w;
    int num_data_items;
    const void** data_ptrs;     /* number of subimages can be big (e.g. array/3d textures)... */
    const int* data_sizes;      /* ...so don't embed fixed-size-arrays here */
    uint32_t _end_canary;
} sg_image_desc;

/*
    sg_shader_desc

    The structure sg_shader_desc describes the shaders, uniform blocks
    and texture images on the vertex- and fragment-shader stage.

    TODO: source code vs byte code, 3D backend API specifics.
*/
typedef struct {
    const char* name;
    int offset;
    sg_uniform_type type;
    int array_count; 
} sg_shader_uniform_desc;

typedef struct {
    int size;
    sg_shader_uniform_desc uniforms[SG_MAX_UB_MEMBERS];
} sg_shader_uniform_block_desc;

typedef struct {
    const char* name;
    sg_image_type type;
} sg_shader_image_desc;

typedef struct {
    const char* source;
    sg_shader_uniform_block_desc uniform_blocks[SG_MAX_SHADERSTAGE_UBS];
    sg_shader_image_desc images[SG_MAX_SHADERSTAGE_IMAGES];
} sg_shader_stage_desc;

typedef struct {
    uint32_t _start_canary;
    sg_shader_stage_desc vs;
    sg_shader_stage_desc fs;
    uint32_t _end_canary;
} sg_shader_desc;

/*
    sg_pipeline_desc

    The sg_pipeline_desc struct defines all creation parameters
    for an sg_pipeline object, used as argument to the
    sg_make_pipeline() function:

    - the complete vertex layout for all input vertex buffers
    - a shader object
    - the 3D primitive type (points, lines, triangles, ...)
    - the index type (none, 16- or 32-bit)
    - depth-stencil state
    - alpha-blending state
    - rasterizer state

    The default configuration is as follows:

    .vertex_layouts[]:
        .stride:        0 (must be initialized to > 0)
        .step_func      SG_VERTEXSTEP_PER_VERTEX
        .step_rate      1
        .attrs[]:
            .name       0 (GLES2 requires an attribute name here)
            .index      0 (optional attribute slot if no name given)
            .offset     0
            .format     SG_VERTEXFORMAT_INVALID (must be initialized!)
    .shader:            0 (must be intilized with a valid sg_shader id!)
    .primitive_type:    SG_PRIMITIVETYPE_TRIANGLES
    .index_type:        SG_INDEXTYPE_NONE
    .depth_stencil:
        .stencil_front, .stencil_back:
            .fail_op:               SG_STENCILOP_KEEP
            .depth_fail_op:         SG_STENCILOP_KEEP
            .pass_op:               SG_STENCILOP_KEEP
            .compare_func           SG_COMPAREFUNC_ALWAYS
        .depth_compare_func:    SG_COMPAREFUNC_ALWAYS
        .depth_write_enabled:   false
        .stencil_enabled:       false
        .stencil_read_mask:     0
        .stencil_write_mask:    0
        .stencil_ref:           0
    .blend:
        .enabled:               false
        .src_factor_rgb:        SG_BLENDFACTOR_ONE
        .dst_factor_rgb:        SG_BLENDFACTOR_ZERO
        .op_rgb:                SG_BLENDOP_ADD
        .src_factor_alpha:      SG_BLENDFACTOR_ONE
        .dst_factor_alpha:      SG_BLENDFACTOR_ZERO
        .op_rgb:                SG_BLENDOP_ADD
        .color_write_mask:      SG_COLORMASK_RGBA
        .blend_color:           { 0.0f, 0.0f, 0.0f, 0.0f }
    .rasterizer:
        .scissor_test_enabled:          false
        .alpha_to_coverage_enabled:     false
        .cull_mode:                     SG_CULLMODE_NONE
        .face_winding:                  SG_FACEWINDING_CW
        .sample_count:                  1

*/
typedef struct {
    const char* name;
    int index;
    int offset;
    sg_vertex_format format;
} sg_vertex_attr_desc;

typedef struct {
    int stride;
    sg_vertex_step step_func;
    int step_rate;
    sg_vertex_attr_desc attrs[SG_MAX_VERTEX_ATTRIBUTES];
} sg_vertex_layout_desc;

typedef struct {
    sg_stencil_op fail_op;
    sg_stencil_op depth_fail_op;
    sg_stencil_op pass_op;
    sg_compare_func compare_func;
} sg_stencil_state;

typedef struct {
    sg_stencil_state stencil_front;
    sg_stencil_state stencil_back;
    sg_compare_func depth_compare_func;
    bool depth_write_enabled;
    bool stencil_enabled;
    uint8_t stencil_read_mask;
    uint8_t stencil_write_mask;
    uint8_t stencil_ref;
} sg_depth_stencil_state;

typedef struct {
    bool enabled;
    sg_blend_factor src_factor_rgb;
    sg_blend_factor dst_factor_rgb;
    sg_blend_op op_rgb;
    sg_blend_factor src_factor_alpha;
    sg_blend_factor dst_factor_alpha;
    sg_blend_op op_alpha;
    uint8_t color_write_mask;
    float blend_color[4];
} sg_blend_state;

typedef struct {
    bool scissor_test_enabled;
    bool alpha_to_coverage_enabled;
    sg_cull_mode cull_mode;
    sg_face_winding face_winding;
    int sample_count;
} sg_rasterizer_state;

typedef struct {
    uint32_t _start_canary;
    sg_vertex_layout_desc vertex_layouts[SG_MAX_SHADERSTAGE_BUFFERS];
    sg_shader shader;
    sg_primitive_type primitive_type;
    sg_index_type index_type;
    sg_depth_stencil_state depth_stencil;
    sg_blend_state blend;
    sg_rasterizer_state rasterizer;
    uint32_t _end_canary;
} sg_pipeline_desc;

/*
    sg_pass_desc

    Creation parameters for an sg_pass object, used as argument
    to the sg_make_pass() function.

    A pass object contains 1..4 color-attachments and none, or one,
    depth-stencil-attachment. Each attachment consists of
    an image, and two additional indices describing
    which subimage the pass will render: one mipmap index, and
    if the image is a cubemap, array-texture or 3D-texture, the
    face-index, array-layer or depth-slice.

    Pass images must fullfill the following requirements:

    All images must have:
    - been created as render target (sg_image_desc.render_target = true)
    - the same size
    - the same sample count
    
    In addition, all color-attachment images must have the same
    pixel format.
*/
typedef struct {
    sg_image image;
    uint16_t mip_level;
    union {
        uint16_t face;
        uint16_t layer;
        uint16_t slice;
    };
} sg_attachment_desc;

typedef struct {
    uint32_t _start_canary;
    sg_attachment_desc color_attachments[SG_MAX_COLOR_ATTACHMENTS];
    sg_attachment_desc depth_stencil_attachment;
    uint32_t _end_canary;
} sg_pass_desc;

/* setup and misc functions */
extern void sg_setup(const sg_desc* desc);
extern void sg_shutdown();
extern bool sg_isvalid();
extern bool sg_query_feature(sg_feature feature);
extern void sg_reset_state_cache();

/* resource creation, destruction and updating */
extern sg_buffer sg_make_buffer(const sg_buffer_desc* desc);
extern sg_image sg_make_image(const sg_image_desc* desc);
extern sg_shader sg_make_shader(const sg_shader_desc* desc);
extern sg_pipeline sg_make_pipeline(const sg_pipeline_desc* desc);
extern sg_pass sg_make_pass(const sg_pass_desc* desc);
extern void sg_destroy_buffer(sg_buffer buf);
extern void sg_destroy_image(sg_image img);
extern void sg_destroy_shader(sg_shader shd);
extern void sg_destroy_pipeline(sg_pipeline pip);
extern void sg_destroy_pass(sg_pass pass);
extern void sg_update_buffer(sg_buffer buf, const void* data_ptr, int data_size);
extern void sg_update_image(sg_image img, int num_data_items, const void** data_ptrs, int* data_sizes); 

/* rendering functions */
extern void sg_begin_default_pass(const sg_pass_action* pass_action, int width, int height);
extern void sg_begin_pass(sg_pass pass, const sg_pass_action* pass_action);
extern void sg_apply_viewport(int x, int y, int width, int height, bool origin_top_left);
extern void sg_apply_scissor_rect(int x, int y, int width, int height, bool origin_top_left);
extern void sg_apply_draw_state(const sg_draw_state* ds);
extern void sg_apply_uniform_block(sg_shader_stage stage, int ub_index, const void* data, int num_bytes);
extern void sg_draw(int base_element, int num_elements, int num_instances);
extern void sg_end_pass();
extern void sg_commit();

/* separate resource allocation and initialization (for async setup) */ 
extern sg_buffer sg_alloc_buffer();
extern sg_image sg_alloc_image();
extern sg_shader sg_alloc_shader();
extern sg_pipeline sg_alloc_pipeline();
extern sg_pass sg_alloc_pass();
extern void sg_init_buffer(sg_buffer buf_id, const sg_buffer_desc* desc);
extern void sg_init_image(sg_image img_id, const sg_image_desc* desc);
extern void sg_init_shader(sg_shader shd_id, const sg_shader_desc* desc);
extern void sg_init_pipeline(sg_pipeline pip_id, const sg_pipeline_desc* desc);
extern void sg_init_pass(sg_pass pass_id, const sg_pass_desc* desc);

/* struct setup helper methods (useful for C++) */
extern sg_vertex_attr_desc sg_named_attr(const char* name, int offset, sg_vertex_format format);
extern sg_shader_uniform_desc sg_named_uniform(const char* name, int offset, sg_uniform_type type, int array_count);
extern sg_shader_image_desc sg_named_image(const char* name, sg_image_type type);

#ifdef __cplusplus
} /* extern "C" */
#endif

#ifdef SOKOL_IMPL
#define SOKOL_IMPL_GUARD
#include "_sokol_gfx.impl.h"
#undef SOKOL_IMPL_GUARD
#endif

