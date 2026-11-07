// pc_gpu.h - the renderer calls the port makes outside the GX core (pc_gx*.c) and the render target.
// One implementation per backend: pc_gpu_gl.c (OpenGL 3.3); the Vulkan backend (pc_gpu_sdl.c, pc_gx_sdl.c) has
// the texture calls and read_pixels so far, called from pc_gpu_gl.c. No GL types here.
#ifndef PC_GPU_H
#define PC_GPU_H

#ifdef __cplusplus
extern "C" {
#endif

// The backend this run uses (settings.ini renderer / --renderer, set before the window exists)
enum { PC_RENDERER_OPENGL = 0, PC_RENDERER_VULKAN = 1 };
extern int g_pc_renderer;
// GL code runs only on the OpenGL backend; the Vulkan backend skips it until it has its own version
static inline int pc_gpu_is_gl(void) { return g_pc_renderer == PC_RENDERER_OPENGL; }

// SDL_GPU backend (pc_gpu_sdl.c, Vulkan). init after the window exists, 0 = failed (then use GL)
struct SDL_Window;
int pc_gpu_sdl_init(struct SDL_Window* window);
// One frame: replay the recorded draws (pc_gx_sdl.c), blit them to the window and present
void pc_gpu_sdl_present(void);
// Swapchain present mode: vsync, else immediate (or mailbox where immediate is missing)
void pc_gpu_sdl_set_vsync(int on);
void pc_gpu_sdl_shutdown(void);

typedef unsigned int PCGpuTexture; // 0 = none (GL: the texture name)
typedef unsigned int PCGpuTarget;  // something drawn into / read from; 0 = the window (GL: the framebuffer name)

typedef enum {
    PC_GPU_RGBA8,      // 4 bytes per pixel
    PC_GPU_RGB565_REV, // 16-bit pixels, red in the low 5 bits (fixNES output)
    PC_GPU_DXT1,       // compressed (texture packs): data_size bytes
    PC_GPU_DXT5,
    PC_GPU_BC7,
} PCGpuFormat;

typedef enum { PC_GPU_NEAREST, PC_GPU_LINEAR } PCGpuFilter;
typedef enum { PC_GPU_CLAMP, PC_GPU_REPEAT, PC_GPU_MIRROR } PCGpuWrap; // GX_CLAMP / GX_REPEAT / GX_MIRROR order

// After the GL context exists: loads the functions, default state. 0 = failed
int pc_gpu_init(void);
// "vendor | renderer | version" for crash reports
void pc_gpu_device_info(char* out, int size);
int pc_gpu_format_supported(PCGpuFormat format);

// Textures are bound on the active texture unit while being set up (callers invalidate their bind caches)
// Uploads (create with data, upload): the data is copied before the call returns (callers free it right after)
// and the image is valid for draws issued after the call. A backend may defer the copy: GL uploads at once,
// SDL_GPU can stage it in a transfer buffer and copy before the render pass. So never upload a texture that a
// draw earlier in the same frame used (today only fixNES re-uploads, once a frame before its draw).
PCGpuTexture pc_gpu_texture_create(int w, int h, PCGpuFormat format, const void* data, int data_size);
// Replaces the whole image (size and format may change)
void pc_gpu_texture_upload(PCGpuTexture tex, int w, int h, PCGpuFormat format, const void* data, int data_size);
void pc_gpu_texture_set_sampler(PCGpuTexture tex, PCGpuFilter filter, PCGpuWrap wrap_s, PCGpuWrap wrap_t);
void pc_gpu_texture_destroy(PCGpuTexture tex);
// The same on SDL_GPU (pc_gpu_sdl.c), called from the pc_gpu_texture_* functions on Vulkan
int pc_gpu_sdl_format_supported(PCGpuFormat format);
PCGpuTexture pc_gpu_sdl_texture_create(int w, int h, PCGpuFormat format, const void* data, int data_size);
void pc_gpu_sdl_texture_upload(PCGpuTexture tex, int w, int h, PCGpuFormat format, const void* data, int data_size);
void pc_gpu_sdl_texture_set_sampler(PCGpuTexture tex, PCGpuFilter filter, PCGpuWrap wrap_s, PCGpuWrap wrap_t);
void pc_gpu_sdl_texture_destroy(PCGpuTexture tex);

// The draw target, returns the previous one
PCGpuTarget pc_gpu_set_draw_target(PCGpuTarget target);
// Clears the draw target (target_w x target_h) to black and draws the texture into the rectangle (bottom-left
// origin), no depth / blend. Changes viewport, scissor, program, texture unit 0: GX state needs restoring after
void pc_gpu_draw_texture(PCGpuTexture tex, int x, int y, int w, int h, int target_w, int target_h);
// RGB rows bottom-up, w * 3 bytes each; caller frees. NULL if out of memory
unsigned char* pc_gpu_read_pixels(PCGpuTarget source, int w, int h);

#ifdef __cplusplus
}
#endif

#endif
