// Texture.cpp: implementation of the CTexture class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include <algorithm>
#include <gli/gli.hpp>
#include <gli/core/bc.hpp>
#include <gli/core/s3tc.hpp>

namespace
{
#ifdef XR_RENDER_GLES
static xr_vector<GLenum> unsupported_compressed_formats;

static bool is_compressed_format_supported(GLenum internalFormat)
{
    static xr_vector<GLint> supportedFormats;
    static bool initialized = false;
    if (!initialized)
    {
        GLint numFormats = 0;
        glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &numFormats);
        if (numFormats > 0)
        {
            supportedFormats.resize(numFormats);
            glGetIntegerv(GL_COMPRESSED_TEXTURE_FORMATS, supportedFormats.data());
        }
        initialized = true;
    }
    for (GLenum fmt : unsupported_compressed_formats)
    {
        if (fmt == internalFormat)
            return false;
    }
    for (GLint fmt : supportedFormats)
    {
        if (static_cast<GLenum>(fmt) == internalFormat)
            return true;
    }
    return false;
}

static void mark_compressed_format_unsupported(GLenum internalFormat)
{
    for (GLenum fmt : unsupported_compressed_formats)
    {
        if (fmt == internalFormat)
            return;
    }
    unsupported_compressed_formats.push_back(internalFormat);
}
#else
static bool is_compressed_format_supported(GLenum /*internalFormat*/)
{
    return true;
}

static void mark_compressed_format_unsupported(GLenum /*internalFormat*/)
{
}
#endif

static bool can_decompress_compressed_format(gli::format format, gli::target target)
{
    if (target != gli::TARGET_2D && target != gli::TARGET_CUBE)
        return false;

    switch (format)
    {
    case gli::FORMAT_RGB_DXT1_UNORM_BLOCK8:
    case gli::FORMAT_RGB_DXT1_SRGB_BLOCK8:
    case gli::FORMAT_RGBA_DXT1_UNORM_BLOCK8:
    case gli::FORMAT_RGBA_DXT1_SRGB_BLOCK8:
    case gli::FORMAT_RGBA_DXT3_UNORM_BLOCK16:
    case gli::FORMAT_RGBA_DXT3_SRGB_BLOCK16:
    case gli::FORMAT_RGBA_DXT5_UNORM_BLOCK16:
    case gli::FORMAT_RGBA_DXT5_SRGB_BLOCK16:
    case gli::FORMAT_R_ATI1N_UNORM_BLOCK8:
    case gli::FORMAT_R_ATI1N_SNORM_BLOCK8:
    case gli::FORMAT_RG_ATI2N_UNORM_BLOCK16:
    case gli::FORMAT_RG_ATI2N_SNORM_BLOCK16:
        return true;
    default:
        return false;
    }
}

static void decompress_compressed_level(gli::format format, const void* src_data, size_t width, size_t height, u8* dst_data)
{
    const bool is_dxt1 = (format == gli::FORMAT_RGB_DXT1_UNORM_BLOCK8 || format == gli::FORMAT_RGB_DXT1_SRGB_BLOCK8 ||
                          format == gli::FORMAT_RGBA_DXT1_UNORM_BLOCK8 || format == gli::FORMAT_RGBA_DXT1_SRGB_BLOCK8);
    const bool is_dxt3 = (format == gli::FORMAT_RGBA_DXT3_UNORM_BLOCK16 || format == gli::FORMAT_RGBA_DXT3_SRGB_BLOCK16);
    const bool is_dxt5 = (format == gli::FORMAT_RGBA_DXT5_UNORM_BLOCK16 || format == gli::FORMAT_RGBA_DXT5_SRGB_BLOCK16);
    const bool is_bc4_unorm = (format == gli::FORMAT_R_ATI1N_UNORM_BLOCK8);
    const bool is_bc4_snorm = (format == gli::FORMAT_R_ATI1N_SNORM_BLOCK8);
    const bool is_bc5_unorm = (format == gli::FORMAT_RG_ATI2N_UNORM_BLOCK16);
    const bool is_bc5_snorm = (format == gli::FORMAT_RG_ATI2N_SNORM_BLOCK16);

    const size_t block_size = (is_dxt1 || is_bc4_unorm || is_bc4_snorm) ? 8 : 16;
    const size_t blocks_x = (width + 3) / 4;
    const size_t blocks_y = (height + 3) / 4;
    const u8* src_bytes = reinterpret_cast<const u8*>(src_data);

    for (size_t by = 0; by < blocks_y; ++by)
    {
        for (size_t bx = 0; bx < blocks_x; ++bx)
        {
            const size_t block_offset = (by * blocks_x + bx) * block_size;
            gli::detail::texel_block4x4 block;

            if (is_dxt1)
            {
                block = gli::detail::decompress_dxt1_block(*reinterpret_cast<const gli::detail::dxt1_block*>(src_bytes + block_offset));
            }
            else if (is_dxt3)
            {
                block = gli::detail::decompress_dxt3_block(*reinterpret_cast<const gli::detail::dxt3_block*>(src_bytes + block_offset));
            }
            else if (is_dxt5)
            {
                block = gli::detail::decompress_dxt5_block(*reinterpret_cast<const gli::detail::dxt5_block*>(src_bytes + block_offset));
            }
            else if (is_bc4_unorm)
            {
                block = gli::detail::decompress_bc4unorm_block(*reinterpret_cast<const gli::detail::bc4_block*>(src_bytes + block_offset));
            }
            else if (is_bc4_snorm)
            {
                block = gli::detail::decompress_bc4snorm_block(*reinterpret_cast<const gli::detail::bc4_block*>(src_bytes + block_offset));
            }
            else if (is_bc5_unorm)
            {
                block = gli::detail::decompress_bc5unorm_block(*reinterpret_cast<const gli::detail::bc5_block*>(src_bytes + block_offset));
            }
            else if (is_bc5_snorm)
            {
                block = gli::detail::decompress_bc5snorm_block(*reinterpret_cast<const gli::detail::bc5_block*>(src_bytes + block_offset));
            }

            const size_t max_r = std::min<size_t>(4, height - by * 4);
            const size_t max_c = std::min<size_t>(4, width - bx * 4);

            for (size_t r = 0; r < max_r; ++r)
            {
                const size_t py = by * 4 + r;
                u8* dst_row = dst_data + (py * width + bx * 4) * 4;
                for (size_t c = 0; c < max_c; ++c)
                {
                    const glm::vec4& t = block.Texel[r][c];
                    dst_row[c * 4 + 0] = static_cast<u8>(std::clamp(t.r * 255.0f + 0.5f, 0.0f, 255.0f));
                    dst_row[c * 4 + 1] = static_cast<u8>(std::clamp(t.g * 255.0f + 0.5f, 0.0f, 255.0f));
                    dst_row[c * 4 + 2] = static_cast<u8>(std::clamp(t.b * 255.0f + 0.5f, 0.0f, 255.0f));
                    dst_row[c * 4 + 3] = static_cast<u8>(std::clamp(t.a * 255.0f + 0.5f, 0.0f, 255.0f));
                }
            }
        }
    }
}
} // namespace

namespace xray::render::RENDER_NAMESPACE
{
void fix_texture_name(pstr fn)
{
    pstr _ext = strext(fn);
    if (_ext &&
        (0 == xr_stricmp(_ext, ".tga") ||
            0 == xr_stricmp(_ext, ".dds") ||
            0 == xr_stricmp(_ext, ".bmp") ||
            0 == xr_stricmp(_ext, ".ogm")))
        *_ext = 0;
}

int get_texture_load_lod(LPCSTR fn)
{
    CInifile::Sect& sect = pSettings->r_section("reduce_lod_texture_list");

    for (const auto& item : sect.Data)
    {
        if (strstr(fn, item.first.c_str()))
        {
            if (psTextureLOD < 1)
                return 0;
            if (psTextureLOD < 3)
                return 1;
            return 2;
        }
    }

    if (psTextureLOD < 2)
        return 0;
    if (psTextureLOD < 4)
        return 1;
    return 2;
}

u32 calc_texture_size(int lod, u32 mip_cnt, size_t orig_size)
{
    if (1 == mip_cnt)
        return orig_size;

    int _lod = lod;
    float res = float(orig_size);

    while (_lod > 0)
    {
        --_lod;
        res -= res / 1.333f;
    }
    return iFloor(res);
}

GLuint CRender::texture_load(pcstr fRName, u32& ret_msize, GLenum& ret_desc, u32& ret_width, u32& ret_height)
{
    ret_msize = 0;
    R_ASSERT1_CURE(fRName && fRName[0], { return 0; });

    GLuint pTexture = 0;
    string_path fn;
    {
        // make file name
        string_path fname;
        xr_strcpy(fname, fRName);
        fix_texture_name(fname);

        // Call to FS.exist WRITES to fn !

        if (!FS.exist(fn, "$game_textures$", fname, ".dds") && strstr(fname, "_bump"))
        {
            Msg("! Fallback to default bump map: %s", fname);
            if (strstr(fname, "_bump#"))
                R_ASSERT1_CURE(FS.exist(fn, "$game_textures$", "ed\\ed_dummy_bump#", ".dds"), return 0);
            else
                R_ASSERT1_CURE(FS.exist(fn, "$game_textures$", "ed\\ed_dummy_bump", ".dds"), return 0);
        }
        else
        {
            bool exist = false;

            for (cpcstr folder : { "$level$", "$game_saves$", "$game_textures$" })
            {
                exist = FS.exist(fn, folder, fname, ".dds");
                if (exist)
                    break;
            }

            if (!exist)
            {
                Msg("! Can't find texture '%s'", fname);
                if (!FS.exist(fn, "$game_textures$", "ed\\ed_not_existing_texture", ".dds"))
                    return 0;
            }
        }
    }

    // Load and get header
    IReader* S = FS.r_open(fn);
    R_ASSERT2_CURE(S, fn, { return 0; });
    size_t img_size = S->length();
#ifdef DEBUG
    Msg("* Loaded: %s[%d]b", fn, img_size);
#endif // DEBUG
    gli::texture texture = gli::load((char*)S->pointer(), img_size);
    R_ASSERT2(!texture.empty(), fn);

    u32 mip_cnt = u32(-1); // XXX: write to it when reading with GLI!

#ifdef XR_RENDER_GLES
    gli::gl GL(gli::gl::PROFILE_ES30);
#else
    gli::gl GL(gli::gl::PROFILE_GL33);
#endif

    gli::gl::format const format = GL.translate(texture.format(), texture.swizzles());
    GLenum target = GL.translate(texture.target());


    const texture_upload_unit uploadUnit(target);

#ifdef XR_RENDER_GLES
    const bool swapRedBlue = !gli::is_compressed(texture.format()) && gli::block_size(texture.format()) == 4
        && format.Swizzles[gli::SWIZZLE_RED] == GL_BLUE;
    xr_vector<u8> reordered;
    const auto texels = [&](size_t layer, size_t face, size_t level) -> const void*
    {
        const void* data = texture.data(layer, face, level);
        if (!swapRedBlue)
            return data;
        const size_t size = texture.size(level);
        reordered.assign(static_cast<const u8*>(data), static_cast<const u8*>(data) + size);
        for (size_t i = 0; i + 3 < size; i += 4)
            std::swap(reordered[i], reordered[i + 2]);
        return reordered.data();
    };
#else
    const auto texels = [&](size_t layer, size_t face, size_t level) { return texture.data(layer, face, level); };
#endif

    glm::tvec3<GLsizei> const tex_extent(texture.extent());

    bool decompress_compressed = gli::is_compressed(texture.format()) &&
        can_decompress_compressed_format(texture.format(), texture.target()) &&
        !is_compressed_format_supported(format.Internal);

    static bool logged_s3tc_fallback = false;

    while (true)
    {
        if (decompress_compressed && !logged_s3tc_fallback)
        {
            Msg("* S3TC not supported, decompressing compressed textures to RGBA8");
            logged_s3tc_fallback = true;
        }

        const bool upload_compressed = gli::is_compressed(texture.format()) && !decompress_compressed;
        const GLenum upload_internal_format = decompress_compressed
            ? (gli::is_srgb(texture.format()) ? GL_SRGB8_ALPHA8 : GL_RGBA8)
            : format.Internal;
        const GLenum upload_external_format = decompress_compressed ? GL_RGBA : format.External;
        const GLenum upload_type = decompress_compressed ? GL_UNSIGNED_BYTE : format.Type;

        while (glGetError() != GL_NO_ERROR);

        glGenTextures(1, &pTexture);
        glBindTexture(target, pTexture);

        glTexParameteri(target, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, static_cast<GLint>(texture.levels() - 1));

#ifndef XR_RENDER_GLES
        if (!decompress_compressed && gli::gl::EXTERNAL_RED != format.External)
            glTexParameteriv(target, GL_TEXTURE_SWIZZLE_RGBA, &format.Swizzles[gli::SWIZZLE_RED]);
#endif

        GLenum err;
        bool use_tex_storage = true;
        bool upload_failed = false;

        switch (texture.target())
        {
        case gli::TARGET_2D:
        case gli::TARGET_CUBE:
            glTexStorage2D(target, static_cast<GLint>(texture.levels()), upload_internal_format,
                           tex_extent.x, tex_extent.y);
            err = glGetError();
            if (err != GL_NO_ERROR)
            {
                use_tex_storage = false;
                if (upload_compressed && can_decompress_compressed_format(texture.format(), texture.target()) &&
                    err == GL_INVALID_ENUM)
                {
                    mark_compressed_format_unsupported(format.Internal);
                    upload_failed = true;
                }
            }
            break;
        case gli::TARGET_3D:
        case gli::TARGET_CUBE_ARRAY:
            glTexStorage3D(target, static_cast<GLint>(texture.levels()), upload_internal_format,
                           tex_extent.x, tex_extent.y, tex_extent.z);
            err = glGetError();
            if (err != GL_NO_ERROR)
            {
                use_tex_storage = false;
                if (upload_compressed && can_decompress_compressed_format(texture.format(), texture.target()) &&
                    err == GL_INVALID_ENUM)
                {
                    mark_compressed_format_unsupported(format.Internal);
                    upload_failed = true;
                }
            }
            break;
        default:
            NODEFAULT;
            break;
        }

        for (size_t layer = 0; layer < texture.layers() && !upload_failed; ++layer)
        {
            for (size_t face = 0; face < texture.faces() && !upload_failed; ++face)
            {
                for (size_t level = 0; level < texture.levels() && !upload_failed; ++level)
                {
                    glm::tvec3<GLsizei> const tex_level_extent(texture.extent(level));
                    GLenum sub_target = gli::is_target_cube(texture.target())
                             ? static_cast<GLenum>(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face)
                             : target;

                    xr_vector<u8> decompressed_level;
                    const void* upload_data = nullptr;
                    if (decompress_compressed)
                    {
                        decompressed_level.resize(static_cast<size_t>(tex_level_extent.x) * tex_level_extent.y * 4);
                        decompress_compressed_level(texture.format(), texture.data(layer, face, level),
                                                    tex_level_extent.x, tex_level_extent.y, decompressed_level.data());
                        upload_data = decompressed_level.data();
                    }
                    else
                    {
                        upload_data = texels(layer, face, level);
                    }

                    switch (texture.target())
                    {
                    case gli::TARGET_2D:
                    case gli::TARGET_CUBE:
                    {
                        if (upload_compressed)
                        {
                            if (use_tex_storage)
                            {
                                glCompressedTexSubImage2D(sub_target, static_cast<GLint>(level),
                                            0, 0, tex_level_extent.x, tex_level_extent.y,
                                            upload_internal_format, static_cast<GLsizei>(texture.size(level)),
                                            upload_data);
                            }
                            else
                            {
                                glCompressedTexImage2D(sub_target, static_cast<GLint>(level),
                                            upload_internal_format, tex_level_extent.x, tex_level_extent.y, 0,
                                            static_cast<GLsizei>(texture.size(level)),
                                            upload_data);
                            }
                            err = glGetError();
                            if (err != GL_NO_ERROR)
                            {
                                if (can_decompress_compressed_format(texture.format(), texture.target()))
                                {
                                    mark_compressed_format_unsupported(format.Internal);
                                    upload_failed = true;
                                }
                                else
                                {
                                    VERIFY(err == GL_NO_ERROR);
                                    Msg("! OpenGL: 0x%x: Invalid 2D compressed texture: '%s'", err, fn);
                                }
                            }
                        }
                        else
                        {
                            if (use_tex_storage)
                            {
                                glTexSubImage2D(sub_target, static_cast<GLint>(level),
                                            0, 0, tex_level_extent.x, tex_level_extent.y,
                                            upload_external_format, upload_type,
                                            upload_data);
                            }
                            else
                            {
                                glTexImage2D(sub_target, static_cast<GLint>(level),
                                            upload_internal_format, tex_level_extent.x, tex_level_extent.y, 0,
                                            upload_external_format, upload_type,
                                            upload_data);
                            }
                            err = glGetError();
                            if (err != GL_NO_ERROR)
                            {
                                VERIFY(err == GL_NO_ERROR);
                                Msg("! OpenGL: 0x%x: Invalid 2D texture: '%s'", err, fn);
                            }
                        }
                        break;
                    }
                    case gli::TARGET_3D:
                    case gli::TARGET_CUBE_ARRAY:
                    {
                        if (upload_compressed)
                        {
                            if (use_tex_storage)
                            {
                                glCompressedTexSubImage3D(target, static_cast<GLint>(level),
                                            0, 0, 0, tex_level_extent.x, tex_level_extent.y, tex_level_extent.z,
                                            upload_internal_format, static_cast<GLsizei>(texture.size(level)),
                                            upload_data);
                            }
                            else
                            {
                                glCompressedTexImage3D(target, static_cast<GLint>(level),
                                            upload_internal_format, tex_level_extent.x, tex_level_extent.y, tex_level_extent.z, 0,
                                            static_cast<GLsizei>(texture.size(level)),
                                            upload_data);
                            }
                            err = glGetError();
                            if (err != GL_NO_ERROR)
                            {
                                if (can_decompress_compressed_format(texture.format(), texture.target()))
                                {
                                    mark_compressed_format_unsupported(format.Internal);
                                    upload_failed = true;
                                }
                                else
                                {
                                    VERIFY(err == GL_NO_ERROR);
                                    Msg("! OpenGL: 0x%x: Invalid compressed 3D texture: '%s'", err, fn);
                                }
                            }
                        }
                        else
                        {
                            if (use_tex_storage)
                            {
                                glTexSubImage3D(target, static_cast<GLint>(level),
                                            0, 0, 0, tex_level_extent.x, tex_level_extent.y, tex_level_extent.z,
                                            upload_external_format, upload_type,
                                            upload_data);
                            }
                            else
                            {
                                glTexImage3D(target, static_cast<GLint>(level),
                                            upload_internal_format, tex_level_extent.x, tex_level_extent.y, tex_level_extent.z, 0,
                                            upload_external_format, upload_type,
                                            upload_data);
                            }
                            err = glGetError();
                            if (err != GL_NO_ERROR)
                            {
                                VERIFY(err == GL_NO_ERROR);
                                Msg("! OpenGL: 0x%x: Invalid 3D texture: '%s'", err, fn);
                            }
                        }
                        break;
                    }
                    default:
                        NODEFAULT;
                        break;
                    }
                }
            }
        }

        if (upload_failed)
        {
            while (glGetError() != GL_NO_ERROR);
            glDeleteTextures(1, &pTexture);
            pTexture = 0;
            decompress_compressed = true;
            continue;
        }

        break;
    }

    FS.r_close(S);

    xr_strlwr(fn);
    ret_desc = target;
    ret_width = tex_extent.x;
    ret_height = tex_extent.y;
    int img_loaded_lod = is_target_cube(texture.target()) ? 0 : get_texture_load_lod(fn);
    ret_msize = calc_texture_size(img_loaded_lod, mip_cnt, img_size);
    return pTexture;
}
} // namespace xray::render::RENDER_NAMESPACE
