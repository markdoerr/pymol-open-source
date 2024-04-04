#pragma once
// -----------------------------------------------------------------------------
#include "GraphicsUtil.h"
#include "Vector.h"
#include <vector>
#include <tuple>
#include <map>
#include <array>
#include <type_traits>
#include <cstdlib>
#include <string.h>
#include <glm/vec2.hpp>

enum class VertexFormat {
  // 8 bit
  Byte,
  Byte2,
  Byte3,
  Byte4,
  ByteNorm,
  Byte2Norm,
  Byte3Norm,
  Byte4Norm,
  UByte,
  UByte2,
  UByte3,
  UByte4,
  UByteNorm,
  UByte2Norm,
  UByte3Norm,
  UByte4Norm,

  //Single Precision
  Float,
  Float2,
  Float3,
  Float4,

  // 32 bit
  Int,
  Int2,
  Int3,
  Int4,
  UInt,
  UInt2,
  UInt3,
  UInt4,
};

enum class VertexFormatBaseType { Byte, UByte, Float, Int, UInt };

/**
 * @param format Vertex format
 * @return The base type of the vertex format
*/
VertexFormatBaseType GetVertexFormatBaseType(VertexFormat format);

/**
 * @param format Vertex format
 * @return The GL type of the vertex format
*/
GLenum VertexFormatToGLType(VertexFormat format);

/**
 * @param format Vertex format
 * @return The number of components in the vertex format
*/
GLint VertexFormatToGLSize(VertexFormat format);

/**
 * @param format Vertex format
 * @return Whether the vertex format is a normalized type
*/
bool VertexFormatIsNormalized(VertexFormat format);

/**
 * @param format Vertex format
 * @return Whether the vertex format is a normalized type as a GLboolean
*/
GLboolean VertexFormatToGLNormalized(VertexFormat format);

/**
 * @param format Vertex format
 * @return The size of the vertex format in bytes
*/
std::size_t GetSizeOfVertexFormat(VertexFormat format);


// -----------------------------------------------------------------------------
// DESCRIPTORS
// -----------------------------------------------------------------------------
// Describes a single array held in the vbo
struct BufferDesc {
  BufferDesc(const char* _attr_name, VertexFormat _format,
      std::size_t _data_size = 0, const void* _data_ptr = nullptr,
      std::uint32_t offset = 0)
      : attr_name(_attr_name)
      , m_format(_format)
      , data_size(_data_size)
      , data_ptr(_data_ptr)
      , offset(offset)
  {
  }

  const char* attr_name{nullptr};
  VertexFormat m_format{VertexFormat::Float};
  std::size_t data_size{};
  const void* data_ptr{nullptr};
  std::uint32_t offset{};
};

using BufferDataDesc = std::vector< BufferDesc >;

/* different types of AttribOp */
enum attrib_op_type {
  NO_COPY = 0,
  FLOAT_TO_FLOAT,
  FLOAT2_TO_FLOAT2,
  FLOAT3_TO_FLOAT3,
  FLOAT4_TO_FLOAT4,
  FLOAT3_TO_UB3,
  FLOAT1_TO_UB_4TH,
  UB3_TO_UB3,
  UINT_INT_TO_PICK_DATA,
  UB1_INTERP_TO_CAP,
  FLOAT1_TO_INTERP,
  UB4_TO_UB4,
  PICK_DATA_TO_PICK_DATA,
  CYL_CAP_TO_CAP,
  FLOAT1_INTERP_TO_CAP,
  UB1_TO_INTERP,
  CYL_CAPS_ARE_ROUND,
  CYL_CAPS_ARE_FLAT,
  CYL_CAPS_ARE_CUSTOM,
  FLOAT4_TO_UB4
};

struct AttribDesc;

typedef void (*AttribOpFuncDataFunctionPtr)(void *varData, const float * pc, void *globalData, int idx);

/* AttribOpFuncData : This structure holds information a callback that sets/post-processes
                      data for a particular attribute.  Currently, a list of these functions
                      are attached to the AttribOp so that when vertices are created (i.e., incr_vertices > 0)
                      then for each vertex, this function is called for the particular attribute attribName.

   funcDataConversion - pointer to function that sets/post-processes the attribute data
   funcDataGlobalArg - pointer to global structure that can be used in each call to the callback
   attribName - attribute name this function is processing. (this calling function specifies the name, and the
                CGOConvertToShader() sets attrib to the associated AttribDesc.
 */
struct AttribOpFuncData {
  void (*funcDataConversion)(void *varData, const float * pc, void *globalData, int idx); // if set, should be called on every output value for this attribute
  void *funcDataGlobalArg;
  const char *attribName;
  AttribDesc *attrib;
  AttribOpFuncDataFunctionPtr _funcDataConversion;
  AttribOpFuncData(AttribOpFuncDataFunctionPtr _funcDataConversion,
                   void *_funcDataGlobalArg,
                   const char *_attribName)
  : funcDataConversion(_funcDataConversion), funcDataGlobalArg(_funcDataGlobalArg), attribName(_attribName), attrib(NULL){}
};

using AttribOpFuncDataDesc = std::vector< AttribOpFuncData >;

/**
 * defines an operation that copies and (optionally) creates new vertices in 
 * a VBO operation for a particular CGO operation (op).
 *
 * op - the CGO operation
 * order - the order for this operation to be executed for the given CGO operation
 * offset - the offset into the CGO operation to copy
 * conv_type - type of copy (can be general or specific, see above, e.g. FLOAT3_TO_FLOAT3, UB1_TO_INTERP)
 * incr_vertices - the number of vertices (if any) that are generated for the VBO after this operation 
 *                 is executed.
 *
 */
struct AttribOp {
  AttribOp(unsigned short _op, size_t _order, size_t _conv_type, size_t _offset, size_t _incr_vertices=0, int _copyFromAttr=-1)
    : op(_op)
    , order(_order)
    , offset(_offset)
    , conv_type(_conv_type)
    , incr_vertices(_incr_vertices)
    , copyFromAttr(_copyFromAttr)
    {}
  unsigned short op { 0 };
  size_t order { 0 };
  size_t offset  { 0 };
  size_t conv_type { 0 };
  size_t incr_vertices { 0 };
  int copyFromAttr { -1 };
  struct AttribDesc *desc { 0 };
  struct AttribDesc *copyAttribDesc { 0 };
  std::vector<AttribOpFuncData> funcDataConversions;
};
using AttribDataOp = std::vector< AttribOp >;

/**
 * defines an attribute that is used in a shader.  this description has all of the necessary information
 * for our "optimize" function to generate either an array for input into the VBO or a call to the
 * related glVertexAttrib() call when this attribute has the same value throughout the CGO.
 *
 * attr_name - the name of this attribute inside the shaders
 * order - order of attribute used in VBO
 * attrOps - all AttribOp for this particular attribute.  This allows the user to define how this
 *           attribute gets populated from the primitive CGO's one or many CGO operations.
 * default_value - pointer to the default value of this attribute (optional, needs to be the same
 *                 size of the attribute's type)
 * repeat_value/repeat_value_length - specified if the attribute has repeating values
 *                                    repeat_value - a pointer to the type and data for repeat values
 *                                    repeat_value_length - number of repeat values
 * type_size - size of type for this attribute (e.g., GL_FLOAT, GL_UNSIGNED_BYTE)
 * type_dim - number of primitives (i.e., type_size) for each vertex of this attribute
 * data_norm - whether this attribute is normalized when passed to the VBO (GL_TRUE or GL_FALSE)
 *
 */
struct AttribDesc {
  AttribDesc(
      const char* _attr_name, VertexFormat _format, AttribDataOp _attrOps = {})
      : attr_name(_attr_name)
      , m_format(_format)
      , attrOps(_attrOps)
  {
  }
  const char * attr_name { nullptr };
  VertexFormat m_format { VertexFormat::Float };
  int order { 0 };
  AttribDataOp attrOps { };
  unsigned char *default_value { nullptr };
  unsigned char *repeat_value { nullptr };
  int repeat_value_length { 0 };
};
using AttribDataDesc = std::vector< AttribDesc >;

class gpuBuffer_t {
  friend class CShaderMgr;
public:
  virtual ~gpuBuffer_t() {};
  virtual size_t get_hash_id() { return _hashid; }
  virtual void bind() const = 0;
protected:
  virtual void set_hash_id(size_t id) { _hashid = id; }
private:
  size_t _hashid { 0 };
};

// -----------------------------------------------------------------------------
/* Vertexbuffer rules:
 * -----------------------------------------------------------------------------
 * - If the buffer data is interleaved then buffer sub data functionality cannot
 *   be used.
 * - The same order of buffer data must be maintained when uploading and binding
 *
 *-----------------------------------------------------------------------
 * USAGE_PATTERN:
 * SEPARATE:
 *   vbo1 [ data1 ]
 *   vbo2 [ data2 ]
 *   ...
 *   vboN [ dataN ]
 * SEQUENTIAL:
 *   vbo [ data1 | data2 | ... | dataN ]
 * INTERLEAVED:
 *   vbo [ data1[0], data2[0], ..., dataN[0] | ... | data1[M], data2[M], ..., dataN[M] ]
 */
template <GLenum _TYPE>
class GenericBuffer : public gpuBuffer_t {
  friend class CShaderMgr;
public:
  static const GLenum TYPE = _TYPE;

  enum buffer_layout {
    SEPARATE,   // multiple vbos
    SEQUENTIAL, // single vbo
    INTERLEAVED // single vbo
  };

  GenericBuffer( buffer_layout layout = SEPARATE, GLenum usage = GL_STATIC_DRAW ) :
    m_buffer_usage(usage), m_layout(layout) {}

  ~GenericBuffer() {
    for (auto i = 0; i < m_desc.size(); ++i) {
      auto& glID = desc_glIDs[i];
      if (glID) {
        glDeleteBuffers(1, &glID);
      }
    }
    if (m_interleavedID) {
      glDeleteBuffers(1, &m_interleavedID);
    }
  }

  /***********************************************************************
   * bufferData
   *----------------------------------------------------------------------
   * Takes a vector of the struct at the top of this file which describes
   * the layout of the vbo object. The supplied data ptr in the struct can
   * be zero, in which case if the default usage is STATIC_DRAW then no
   * opengl buffer will be generated for that, else it is assumed that the
   * data will be supplied at a later point because it's dynamic draw.
   ***********************************************************************/
  bool bufferData(BufferDataDesc && desc) {
    m_desc = std::move(desc);
    desc_glIDs = std::vector<GLuint>(m_desc.size());
    return evaluate();
  }

  bool bufferData(BufferDataDesc && desc, const void * data, size_t len, size_t stride) {
    bool ok = true;
    m_desc = std::move(desc);
    desc_glIDs = std::vector<GLuint>(m_desc.size());
    m_interleaved = true;
    m_stride = stride;
    ok = genBuffer(m_interleavedID, len, data);
    return ok;
  }

  // -----------------------------------------------------------------------------
  // bufferSubData :
  // This function assumes that the data layout hasn't change
  void bufferSubData(size_t offset, size_t size, void * data, size_t index = 0) {
    assert("Invalid Desc index" && index < m_desc.size());
    assert("Invalid GLDesc index" && index < desc_glIDs.size());
    auto glID = m_interleaved ? m_interleavedID : desc_glIDs[index];
    glBindBuffer(TYPE, glID);
    glBufferSubData(TYPE, offset, size, data);
  }

  // for interleaved dat only, replaces the whole interleaved vbo
  void bufferReplaceData(size_t offset, size_t len, const void * data) {
    glBindBuffer(TYPE, m_interleavedID);
    glBufferSubData(TYPE, offset, len, data);
  }


protected:

  bool evaluate() {
    bool ok = true;
    if (TYPE == GL_ELEMENT_ARRAY_BUFFER) {
      ok = seqBufferData();
    } else {
      switch (m_layout) {
      case SEPARATE:
        ok = sepBufferData();
        break;
      case SEQUENTIAL:
        ok = seqBufferData();
        break;
      case INTERLEAVED:
        ok = interleaveBufferData();
        break;
      }
    }
    return ok;
  }

  // USAGE PATTERNS
  bool sepBufferData() {
    for (auto i = 0; i < m_desc.size(); ++i) {
      // If the specified size is 0 but we have a valid pointer
      // then we are going to glVertexAttribXfv X in {1,2,3,4}
      const auto& d = m_desc[i];
      auto& glID = desc_glIDs[i];
      if (d.data_ptr && (m_buffer_usage == GL_STATIC_DRAW)) {
        if (d.data_size) {
          if (!genBuffer(glID, d.data_size, d.data_ptr)) {
            return false;
          }
        }
      }
    }
    return true;
  }

  bool seqBufferData() {
    // this is only going to use a single opengl vbo
    m_interleaved = true;

    size_t buffer_size { 0 };
    for ( auto & d : m_desc ) {
      buffer_size += d.data_size;
    }

    std::vector<std::uint8_t> buffer_data(buffer_size);
    auto data_ptr = buffer_data.data();
    size_t offset = 0;

    for ( auto & d : m_desc ) {
      d.offset = offset;
      if (d.data_ptr)
        memcpy(data_ptr, d.data_ptr, d.data_size);
      else
        memset(data_ptr, 0, d.data_size);
      data_ptr += d.data_size;
      offset += d.data_size;
    }

    return genBuffer(m_interleavedID, buffer_size, buffer_data.data());
  }

  bool interleaveBufferData() {
    size_t interleaved_size = 0;
    const size_t buffer_count = m_desc.size();
    size_t stride = 0;
    std::vector<uint8_t *> data_table(buffer_count);
    std::vector<uint8_t *> ptr_table(buffer_count);
    std::vector<size_t> size_table(buffer_count);
    size_t count = m_desc[0].data_size / GetSizeOfVertexFormat(m_desc[0].m_format);

    // Maybe assert that all pointers in d_desc are valid?
    for ( size_t i = 0; i < buffer_count; ++i ) {
      auto &d = m_desc[i];
      // offset is the current stride
      d.offset = stride;

      // These must come after so that offset starts at 0
      // Size of 3 normals or whatever the current type is
      size_table[i] = GetSizeOfVertexFormat(d.m_format);

      // Increase our current estimate of the stride by this amount
      stride += size_table[i];

      // Does the addition of that previous stride leave us on a word boundry?
      int m = stride % 4;
      stride = (m ? (stride + (4 - m)) : stride);

      // data_table a pointer to the begining of each array
      data_table[i] = (uint8_t *)d.data_ptr;

      // We will move these pointers along by the values in the size table
      ptr_table[i] = data_table[i];

    }

    m_stride = stride;

    interleaved_size = count * stride;

    uint8_t *interleaved_data = (uint8_t *)calloc(interleaved_size, sizeof(uint8_t));
    uint8_t *i_ptr = interleaved_data;

    while (i_ptr != (interleaved_data + interleaved_size)) {
      for ( size_t i = 0; i < buffer_count; ++i ) {
        if (ptr_table[i]){
	memcpy( i_ptr, ptr_table[i], size_table[i] );
	ptr_table[i] += size_table[i];
        }
	i_ptr += size_table[i];
      }
    }

    bool ok = true;
    ok = genBuffer(m_interleavedID, interleaved_size, interleaved_data);
    m_interleaved = true;
    free(interleaved_data);
    return ok;
  }

  bool genBuffer(GLuint &id, size_t size, const void * ptr) {
    glGenBuffers(1, &id);
    if (!glCheckOkay())
      return false;
    glBindBuffer(TYPE, id);
    if (!glCheckOkay())
      return false;
    glBufferData(TYPE, size, ptr, GL_STATIC_DRAW);
    if (!glCheckOkay())
      return false;
    return true;
  }

protected:
  bool m_status                { false };
  bool m_interleaved           { false };
  GLuint m_interleavedID       { 0 };
  const GLenum m_buffer_usage  { GL_STATIC_DRAW };
  const buffer_layout m_layout { SEPARATE };
  size_t m_stride              { 0 };
  BufferDataDesc     m_desc;
  std::vector<GLuint> desc_glIDs; // m_desc's gl buffer IDs
};

/**
 * Vertex buffer specialization
 */
class VertexBuffer : public GenericBuffer<GL_ARRAY_BUFFER> {
  void bind_attrib(GLuint prg, const BufferDesc& d, GLuint glID) {
    GLint loc = glGetAttribLocation(prg, d.attr_name);
    auto type_dim = VertexFormatToGLSize(d.m_format);
    auto type = VertexFormatToGLType(d.m_format);
    auto data_norm = VertexFormatToGLNormalized(d.m_format);
    bool masked = false;
    for (GLint lid : m_attribmask)
      if (lid == loc)
        masked = true;
    if ( loc >= 0 )
      m_locs.push_back(loc);
    if ( loc >= 0 && !masked ) {
      if (!m_interleaved && glID)
        glBindBuffer( TYPE, glID);
      glEnableVertexAttribArray( loc );
      glVertexAttribPointer(loc, type_dim, type, data_norm, m_stride,
          reinterpret_cast<const void*>(d.offset));
    }
  };

public:
  VertexBuffer( buffer_layout layout = SEPARATE, GLenum usage = GL_STATIC_DRAW ) : GenericBuffer<GL_ARRAY_BUFFER>(layout, usage){}

  void bind() const {
    // we shouldn't use this one
    if (m_interleaved)
      glBindBuffer(TYPE, m_interleavedID);
  }

  void bind(GLuint prg, int index = -1) {
    if (index >= 0) {
      glBindBuffer( TYPE, m_interleavedID );
      bind_attrib(prg, m_desc[index], desc_glIDs[index]);
    } else {
      if (m_interleaved && m_interleavedID)
        glBindBuffer( TYPE, m_interleavedID );
      for (auto i = 0; i < m_desc.size(); ++i) {
        const auto& d = m_desc[i];
        auto glID = desc_glIDs[i];
        bind_attrib(prg, d, glID);
      }
      m_attribmask.clear();
    }
  }

  void unbind() {
    for (auto &d : m_locs) {
      glDisableVertexAttribArray(d);
    }
    m_locs.clear();
    glBindBuffer(TYPE, 0);
  }

  void maskAttributes(std::vector<GLint> attrib_locs) {
    m_attribmask = std::move(attrib_locs);
  }

  void maskAttribute(GLint attrib_loc) {
    m_attribmask.push_back(attrib_loc);
  }

private:
  // m_locs is only for interleaved data
  std::vector<GLint> m_locs;
  std::vector<GLint> m_attribmask;
};

/**
 * Index buffer specialization
 */
class IndexBuffer : public GenericBuffer<GL_ELEMENT_ARRAY_BUFFER> {
public:
  using GenericBuffer::GenericBuffer;

  void bind() const {
    glBindBuffer(TYPE, m_interleavedID);
  }

  void unbind() {
    glBindBuffer(TYPE, 0);
  }
};

// Forward Decls
class frameBuffer_t;
class renderBuffer_t;

/***********************************************************************
 * RENDERBUFFER
 ***********************************************************************/
namespace rbo {
  enum storage {
    DEPTH16 = 0,
    DEPTH24,
    COUNT
  };

  void unbind();
};

class renderBuffer_t : public gpuBuffer_t {
  friend class frameBuffer_t;
  friend class CShaderMgr;
public:
  renderBuffer_t(int width, int height, rbo::storage storage) :
    _width(width), _height(height), _storage(storage) {
    genBuffer();
  }
  ~renderBuffer_t() {
    freeBuffer();
  }

  void bind() const;
  void unbind() const;

private:
  void genBuffer();
  void freeBuffer();

protected:
  uint32_t     _id;
  int          _dim[2];
  int          _width;
  int          _height;
  rbo::storage _storage;
};

/***********************************************************************
 * TEXTURE
 ***********************************************************************/
namespace tex {
  enum class dim : int {
    D1 = 0,
    D2,
    D3,
    COUNT
  };
  enum class format : int {
    R = (int)dim::COUNT,
    RG,
    RGB,
    RGBA,
    COUNT
  };
  enum class data_type : int {
    UBYTE = (int)format::COUNT,
    FLOAT,
    HALF_FLOAT,
    COUNT
  };
  enum class filter : int {
    NEAREST = (int)data_type::COUNT,
    LINEAR,
    NEAREST_MIP_NEAREST,
    NEAREST_MIP_LINEAR,
    LINEAR_MIP_NEAREST,
    LINEAR_MIP_LINEAR,
    COUNT
  };
  enum class wrap : int {
    REPEAT = (int)filter::COUNT,
    CLAMP,
    MIRROR_REPEAT,
    CLAMP_TO_EDGE,
    CLAMP_TO_BORDER,
    MIRROR_CLAMP_TO_EDGE,
    COUNT
  };
  enum class env_name : int {
    ENV_MODE = (int)wrap::COUNT,
    COUNT
  };
  enum class env_param : int {
    REPLACE = (int)env_name::COUNT,
    COUNT
  };
  const uint32_t max_params = (int)env_param::COUNT;

  void env(tex::env_name, tex::env_param);
};

class textureBuffer_t : public gpuBuffer_t {
  friend class frameBuffer_t;
public:
  // Generates a 1D texture
  textureBuffer_t(tex::format format, tex::data_type type,
                  tex::filter mag, tex::filter min,
                  tex::wrap wrap_s) :
    _dim(tex::dim::D1), _format(format), _type(type),
    _sampling({(int)mag, (int)min, (int)wrap_s, 0, 0})
    {
      genBuffer();
    };
  // Generates a 2D texture
  textureBuffer_t(tex::format format, tex::data_type type,
                  tex::filter mag, tex::filter min,
                  tex::wrap wrap_s, tex::wrap wrap_t) :
    _dim(tex::dim::D2), _format(format), _type(type),
    _sampling({(int)mag, (int)min, (int)wrap_s, (int)wrap_t, 0})
    {
      genBuffer();
    };
  // Generates a 3D texture
  textureBuffer_t(tex::format format, tex::data_type type,
                  tex::filter mag, tex::filter min,
                  tex::wrap wrap_s, tex::wrap wrap_t,
                  tex::wrap wrap_r) :
    _dim(tex::dim::D3), _format(format), _type(type),
    _sampling({(int)mag, (int)min, (int)wrap_s, (int)wrap_t, (int)wrap_r})
    {
      genBuffer();
    };
  ~textureBuffer_t() {
    freeBuffer();
  }

  void bind() const;
  /**
   * Binds the texture to a specific texture unit
   * @param textureUnit The texture unit to bind to (0, 1, 2, etc)
   */
  void bindToTextureUnit(std::uint8_t textureUnit) const;
  void unbind() const;

  void texture_data_1D(int width, const void * data);
  void texture_data_2D(int width, int height, const void * data);
  void texture_data_3D(int width, int height, int depth, const void * data);

  /**
   * Specifies a 2D texture subimage
   * @param xoffset The x offset of the subimage
   * @param yoffset The y offset of the subimage
   * @param width The width of the subimage
   * @param height The height of the subimage
   * @param data The data to upload
  */
  void texture_subdata_2D(int xoffset, int yoffset, int width, int height, const void* data);
private:
  void genBuffer();
  void freeBuffer();

private:
  const tex::dim           _dim;
  const tex::format        _format;
  const tex::data_type     _type;
  const std::array<int, 5> _sampling;
  uint32_t                 _id     { 0 };
  int                      _width  { 0 };
  int                      _height { 0 };
  int                      _depth  { 0 };
};

/***********************************************************************
 * FRAMEBUFFER
 ***********************************************************************/
namespace fbo {
  enum attachment {
    COLOR0 = 0,
    COLOR1,
    COLOR2,
    COLOR3,
    DEPTH,
    COUNT
  };

  // global unbind for fbos
  void unbind();
}

class frameBuffer_t : public gpuBuffer_t {
  friend class CShaderMgr;
public:
  frameBuffer_t() {
    genBuffer();
  }
  ~frameBuffer_t() {
    freeBuffer();
  }

  void attach_texture(textureBuffer_t * texture, fbo::attachment loc);
  void attach_renderbuffer(renderBuffer_t * renderbuffer, fbo::attachment loc);
  void print_fbo();

  void bind() const;
  void unbind() const;
private:
  void genBuffer();
  void freeBuffer();
  void checkStatus();

protected:
  uint32_t _id { 0 };
  std::vector<std::tuple<size_t, fbo::attachment>> _attachments;
};


/***********************************************************************
 * RENDERTARGET
 *----------------------------------------------------------------------
 * A 2D render target that automatically has depth, used for postprocess
 ***********************************************************************/
struct rt_layout_t {
  enum data_type   { UBYTE, FLOAT };
  rt_layout_t(uint8_t _nchannels, data_type _type)
    : nchannels(_nchannels), type(_type) {}
  rt_layout_t(uint8_t _nchannels, data_type _type, int _width, int _height)
    : nchannels(_nchannels), type(_type), width(_width), height(_height) {}
  uint8_t     nchannels;
  data_type   type;
  int         width  { 0 };
  int         height { 0 };
};

class renderTarget_t : public gpuBuffer_t {
  friend class CShaderMgr;
public:
  using shape_type = glm::ivec2;

  renderTarget_t(shape_type size) : _size(size) {}
  renderTarget_t(int width, int height) : _size(width, height) {}
  ~renderTarget_t();

  void bind() const { bind(true); };
  void bind(bool clear) const;
  void bindFBORBO() const
  {
    _fbo->bind();
    _rbo->bind();
  }

  void layout(std::vector<rt_layout_t>&& desc, renderBuffer_t * with_rbo = nullptr);
  void resize(shape_type size);

  const shape_type& size() const { return _size; };

  renderBuffer_t* rbo() const noexcept { return _rbo; }
  frameBuffer_t* fbo() const noexcept { return _fbo; }
  const std::vector<textureBuffer_t*>& textures() const noexcept { return _textures; }

protected:
  bool _shared_rbo { false };
  shape_type _size;
  frameBuffer_t * _fbo;
  renderBuffer_t * _rbo;
  std::vector<rt_layout_t> _desc;
  std::vector<textureBuffer_t *> _textures;
};
