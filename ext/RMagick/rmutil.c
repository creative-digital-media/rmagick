/* $Id: rmutil.c,v 1.23 2004/02/23 00:31:21 rmagick Exp $ */
/*============================================================================\
|                Copyright (C) 2004 by Timothy P. Hunter
| Name:     rmutil.c
| Author:   Tim Hunter
| Purpose:  Utility functions for RMagick
\============================================================================*/

#include "rmagick.h"

static const char *Compliance_Const_Name(ComplianceType *);
static const char *StyleType_Const_Name(StyleType);
static const char *StretchType_Const_Name(StretchType);
static void Color_Name_to_PixelPacket(PixelPacket *, VALUE);



/*
    Extern:     magick_malloc, magick_free, magick_realloc
    Purpose:    ****Magick versions of standard memory routines.
    Notes:      use when managing memory that ****Magick may have
                allocated or may free.
                If malloc fails, it raises an exception
*/
void *magick_malloc(const size_t size)
{
    void *ptr;
#if defined(HAVE_ACQUIREMAGICKMEMORY)
    ptr = AcquireMagickMemory(size);
#else
    ptr = AcquireMemory(size);
#endif
    if (!ptr)
    {
        rb_raise(rb_eNoMemError, "not enough memory to continue");
    }

    return ptr;
}

void magick_free(void *ptr)
{
#if defined(HAVE_ACQUIREMAGICKMEMORY)
    RelinquishMagickMemory(ptr);
#else
    void *v = ptr;
    LiberateMemory(&v);
#endif
}

void *magick_realloc(void *ptr, const size_t size)
{
    void *v;
#if defined(HAVE_ACQUIREMAGICKMEMORY)
    v = ResizeMagickMemory(ptr, size);
#else
    v = ptr;
    ReacquireMemory(&v, size);
#endif
    if (!v)
    {
        rb_raise(rb_eNoMemError, "not enough memory to continue");
    }
    return v;
}

/*
    Extern:     magick_clone_string
    Purpose:    make a copy of a string in malloc'd memory
    Notes:      Any existing string pointed to by *new_str is freed.
                If malloc fails, raises an exception
*/
void magick_clone_string(char **new_str, const char *str)
{
    unsigned int okay;

    okay = CloneString(new_str, str);
    if (!okay)
    {
        rb_raise(rb_eNoMemError, "not enough memory to continue");
    }
}

/*
    Extern:     rm_string_value_ptr(VALUE*)
    Purpose:    emulate Ruby 1.8's rb_string_value_ptr
    Notes:      This is essentially 1.8's rb_string_value_ptr
                with a few minor changes to make it work in 1.6.
                Always called via STRING_PTR
*/
#if !defined StringValuePtr
char *
rm_string_value_ptr(volatile VALUE *ptr)
{
    volatile VALUE s = *ptr;

    // If VALUE is not a string, call to_str on it
    if (TYPE(s) != T_STRING)
    {
       s = rb_str_to_str(s);
       *ptr = s;
    }
    // If ptr == NULL, allocate a 1 char array
    if (!RSTRING(s)->ptr)
    {
        RSTRING(s)->ptr = ALLOC_N(char, 1);
        (RSTRING(s)->ptr)[0] = 0;
        RSTRING(s)->orig = 0;
    }
    return RSTRING(s)->ptr;
}
#endif

/*
    Extern:     rm_string_value_ptr_len
    Purpose:    safe replacement for rb_str2cstr
    Returns:    stores string length in 2nd arg, returns ptr to C string
    Notes:      Uses rb/rm_string_value_ptr to ensure correct String
                argument.
                Always called via STRING_PTR_LEN
*/
char *rm_string_value_ptr_len(volatile VALUE *ptr, long *len)
{
    volatile VALUE v = *ptr;
    char *str;

    str = STRING_PTR(v);
    *ptr = v;
    *len = RSTRING(v)->len;
    return str;
}

/*
    Extern:     ImageList_cur_image
    Purpose:    Sends the "cur_image" method to the object. If 'img'
                is an ImageList, then cur_image is self[@scene].
                If 'img' is an image, then cur_image is simply
                'self'.
    Returns:    the return value from "cur_image"
*/
VALUE
ImageList_cur_image(VALUE img)
{
    return rb_funcall(img, cur_image_ID, 0);
}

/*
    Method:     Magick::PrimaryInfo#to_s
    Purpose:    Create a string representation of a Magick::PrimaryInfo
*/
VALUE
PrimaryInfo_to_s(VALUE self)
{
    PrimaryInfo pi;
    char buff[100];

    PrimaryInfo_to_PrimaryInfo(&pi, self);
    sprintf(buff, "x=%g, y=%g, z=%g", pi.x, pi.y, pi.z);
    return rb_str_new2(buff);
}

/*
    Method:     Magick::Chromaticity#to_s
    Purpose:    Create a string representation of a Magick::Chromaticity
*/
VALUE
ChromaticityInfo_to_s(VALUE self)
{
    ChromaticityInfo ci;
    char buff[200];

    ChromaticityInfo_to_ChromaticityInfo(&ci, self);
    sprintf(buff, "red_primary=(x=%g,y=%g) "
                  "green_primary=(x=%g,y=%g) "
                  "blue_primary=(x=%g,y=%g) "
                  "white_point=(x=%g,y=%g) ",
                  ci.red_primary.x, ci.red_primary.y,
                  ci.green_primary.x, ci.green_primary.y,
                  ci.blue_primary.x, ci.blue_primary.y,
                  ci.white_point.x, ci.white_point.y);
    return rb_str_new2(buff);
}

/*
    Method:     Magick::Pixel#to_s
    Purpose:    Create a string representation of a Magick::Pixel
*/
VALUE
Pixel_to_s(VALUE self)
{
    PixelPacket pp;
    char buff[100];

    Pixel_to_PixelPacket(&pp, self);
    sprintf(buff, "red=%d, green=%d, blue=%d, opacity=%d"
          , pp.red, pp.green, pp.blue, pp.opacity);
    return rb_str_new2(buff);
}

/*
    Method:     Magick::Pixel.from_color(string)
    Purpose:    Construct an Magick::Pixel corresponding to the given color name.
    Notes:      the "inverse" is Image *#to_color, b/c the conversion of a pixel
                to a color name requires both a color depth and if the opacity
                value has meaning (i.e. whether image->matte == True or not).

                Also see Magick::Pixel#to_color, below.
*/
VALUE
Pixel_from_color(VALUE class, VALUE name)
{
    PixelPacket pp;
    ExceptionInfo exception;
    boolean okay;

    class = class;      // defeat "never referenced" message from icc

    GetExceptionInfo(&exception);
    okay = QueryColorDatabase(STRING_PTR(name), &pp, &exception);
    HANDLE_ERROR
    if (!okay)
    {
        rb_raise(rb_eArgError, "invalid color name: %s", STRING_PTR(name));
    }

    return Pixel_from_PixelPacket(&pp);
}

/*
    Method:     Magick::Pixel#to_color(compliance=Magick::???Compliance,
                                      matte=False
                                      depth=QuantumDepth)
    Purpose:    return the color name corresponding to the pixel values
    Notes:      the conversion respects the value of the 'opacity' field
                in the Pixel.
*/
VALUE
Pixel_to_color(int argc, VALUE *argv, VALUE self)
{
    Info *info;
    Image *image;
    PixelPacket pp;
    char name[MaxTextExtent];
    ExceptionInfo exception;
    ComplianceType compliance = AllCompliance;
    unsigned int matte = False;
    unsigned int depth = QuantumDepth;

    switch (argc)
    {
        case 3:
            depth = NUM2UINT(argv[2]);

            // Ensure depth is appropriate for the way xMagick was compiled.
            switch (depth)
            {
                case 8:
#if QuantumDepth == 16 || QuantumDepth == 32
                case 16:
#endif
#if QuantumDepth == 32
                case 32:
#endif
                    break;
                default:
                    rb_raise(rb_eArgError, "invalid depth (%d)", depth);
                    break;
            }
       case 2:
            matte = RTEST(argv[1]);
        case 1:
            VALUE_TO_ENUM(argv[0], compliance, ComplianceType);
        case 0:
            break;
        default:
            rb_raise(rb_eArgError, "wrong number of arguments (%d for 0 to 2)", argc);
    }

    Pixel_to_PixelPacket(&pp, self);

    info = CloneImageInfo(NULL);
    image = AllocateImage(info);
    image->depth = depth;
    image->matte = matte;
    DestroyImageInfo(info);
    GetExceptionInfo(&exception);
    (void) QueryColorname(image, &pp, compliance, name, &exception);
    DestroyImage(image);
    HANDLE_ERROR

    // Always return a string, even if it's ""
    return rb_str_new2(name);
}

/*
    Method:     Pixel#to_HSL
    Purpose:    Converts an RGB pixel to the array
                [hue, saturation, luminosity].
*/
VALUE
Pixel_to_HSL(VALUE self)
{
    PixelPacket rgb;
    double hue, saturation, luminosity;
    volatile VALUE hsl;

    Pixel_to_PixelPacket(&rgb, self);
    TransformHSL(rgb.red, rgb.green, rgb.blue,
                 &hue, &saturation, &luminosity);

    hsl = rb_ary_new3(3, rb_float_new(hue), rb_float_new(saturation),
                      rb_float_new(luminosity));

    return hsl;
}

/*
    Method:     Pixel.from_HSL
    Purpose:    Constructs an RGB pixel from the array
                [hue, saturation, luminosity].
*/
VALUE
Pixel_from_HSL(VALUE class, VALUE hsl)
{
    PixelPacket rgb = {0};
    double hue, saturation, luminosity;

    class = class;      // defeat "never referenced" message from icc

    Check_Type(hsl, T_ARRAY);

    hue        = NUM2DBL(rb_ary_entry(hsl, 0));
    saturation = NUM2DBL(rb_ary_entry(hsl, 1));
    luminosity = NUM2DBL(rb_ary_entry(hsl, 2));

    HSLTransform(hue, saturation, luminosity,
                 &rgb.red, &rgb.green, &rgb.blue);
    return Pixel_from_PixelPacket(&rgb);
}

/*
    Method:  Pixel#fcmp(other[, fuzz[, colorspace]])
    Purpose: Compare pixel values for equality
    Notes:   The colorspace value is ignored < 5.5.5
             and > 5.5.7.
*/
VALUE
Pixel_fcmp(int argc, VALUE *argv, VALUE self)
{
#if defined(HAVE_FUZZYCOLORCOMPARE)
    Image *image;
    Info *info;
#endif

    PixelPacket this, that;
    ColorspaceType colorspace = RGBColorspace;
    double fuzz = 0.0;
    unsigned int equal;

    switch (argc)
    {
        case 3:
            VALUE_TO_ENUM(argv[2], colorspace, ColorspaceType);
        case 2:
            fuzz = NUM2DBL(argv[1]);
        case 1:
            // Allow 1 argument
            break;
        default:
            rb_raise(rb_eArgError, "wrong number of arguments (%d for 1 to 3)", argc);
            break;
    }

    Pixel_to_PixelPacket(&this, self);
    Pixel_to_PixelPacket(&that, argv[0]);

#if defined(HAVE_FUZZYCOLORCOMPARE)
    // The FuzzyColorCompare function expects to get the
    // colorspace and fuzz parameters from an Image structure.

    info = CloneImageInfo(NULL);
    if (!info)
    {
        rb_raise(rb_eNoMemError, "not enough memory to continue");
    }

    image = AllocateImage(info);
    if (!image)
    {
        rb_raise(rb_eNoMemError, "not enough memory to continue");
    }
    DestroyImageInfo(info);

    image->colorspace = colorspace;
    image->fuzz = fuzz;

    equal = FuzzyColorCompare(image, &this, &that);
    DestroyImage(image);

#else
    equal = FuzzyColorMatch(&this, &that, fuzz);
#endif

    return equal ? Qtrue : Qfalse;
}

/*
    Method:  Pixel#intensity
    Purpose: Return the "intensity" of a pixel
*/
VALUE
Pixel_intensity(VALUE self)
{
    PixelPacket pixel;
    unsigned long intensity;

    Pixel_to_PixelPacket(&pixel, self);
    intensity = (unsigned long)
                (0.299*pixel.red) + (0.587*pixel.green) + (0.114*pixel.blue);
    return ULONG2NUM(intensity);
}

/*
    Method:     Magick::Rectangle#to_s
    Purpose:    Create a string representation of a Magick::Rectangle
*/
VALUE
RectangleInfo_to_s(VALUE self)
{
    RectangleInfo rect;
    char buff[100];

    Rectangle_to_RectangleInfo(&rect, self);
    sprintf(buff, "width=%lu, height=%lu, x=%ld, y=%ld"
          , rect.width, rect.height, rect.x, rect.y);
    return rb_str_new2(buff);
}

/*
    Method:     Magick::SegmentInfo#to_s
    Purpose:    Create a string representation of a Magick::Segment
*/
VALUE
SegmentInfo_to_s(VALUE self)
{
    SegmentInfo segment;
    char buff[100];

    Segment_to_SegmentInfo(&segment, self);
    sprintf(buff, "x1=%g, y1=%g, x2=%g, y2=%g"
          , segment.x1, segment.y1, segment.x2, segment.y2);
    return rb_str_new2(buff);
}

/*
    Extern:     PixelPacket_to_Color_Name
    Purpose:    Map the color intensity to a named color
    Returns:    the named color as a String
    Notes:      See below for the equivalent function that accepts an Info
                structure instead of an Image.
*/
VALUE
PixelPacket_to_Color_Name(Image *image, PixelPacket *color)
{
    char name[MaxTextExtent];
    ExceptionInfo exception;

    GetExceptionInfo(&exception);

    (void) QueryColorname(image, color, X11Compliance, name, &exception);
    HANDLE_IMG_ERROR(image)

    return rb_str_new2(name);
}

/*
    Extern:     PixelPacket_to_Color_Name_Info
    Purpose:    Map the color intensity to a named color
    Returns:    the named color as a String
    Notes:      Accepts an Info structure instead of an Image (see above).
                Simply create an Image from the Info, call QueryColorname,
                and then destroy the Image.
                If the Info structure is NULL, creates a new one.

                Note that the default depth is always used, and the matte
                value is set to False, which means "don't use the alpha channel".
*/
VALUE
PixelPacket_to_Color_Name_Info(Info *info, PixelPacket *color)
{
    Image *image;
    Info *my_info;
    volatile VALUE color_name;

    my_info = info ? info : CloneImageInfo(NULL);

    image = AllocateImage(info);
    image->matte = False;
    color_name = PixelPacket_to_Color_Name(image, color);
    DestroyImage(image);
    if (!info)
    {
        DestroyImageInfo(my_info);
    }

    return color_name;
}

/*
    Static:     Color_Name_to_PixelPacket
    Purpose:    Convert a color name to a PixelPacket
    Raises:     ArgumentError
*/
static void
Color_Name_to_PixelPacket(PixelPacket *color, VALUE name_arg)
{
    boolean okay;
    char *name;
    ExceptionInfo exception;

    GetExceptionInfo(&exception);
    name = STRING_PTR(name_arg);
    okay = QueryColorDatabase(name, color, &exception);
    DestroyExceptionInfo(&exception);
    if (!okay)
    {
        rb_raise(rb_eArgError, "invalid color name %s", name);
    }
}

/*
    Extern:     AffineMatrix_from_AffineMatrix
    Purpose:    Create a Magick::AffineMatrix object from an AffineMatrix structure.
*/
VALUE
AffineMatrix_from_AffineMatrix(AffineMatrix *am)
{
    return rb_funcall(Class_AffineMatrix, new_ID, 6
                    , rb_float_new(am->sx), rb_float_new(am->rx), rb_float_new(am->ry)
                    , rb_float_new(am->sy), rb_float_new(am->tx), rb_float_new(am->ty));
}

/*
    Extern:     AffineMatrix_to_AffineMatrix
    Purpose:    Convert a Magick::AffineMatrix object to a AffineMatrix structure.
    Notes:      If not initialized, the defaults are [sx,rx,ry,sy,tx,ty] = [1,0,0,1,0,0]
*/
void
AffineMatrix_to_AffineMatrix(AffineMatrix *am, VALUE st)
{
    volatile VALUE values, v;

    if (CLASS_OF(st) != Class_AffineMatrix)
    {
        rb_raise(rb_eTypeError, "type mismatch: %s given",
                 rb_class2name(CLASS_OF(st)));
    }
    values = rb_funcall(st, values_ID, 0);
    v = rb_ary_entry(values, 0);
    am->sx = v == Qnil ? 1.0 : NUM2DBL(v);
    v = rb_ary_entry(values, 1);
    am->rx = v == Qnil ? 0.0 : NUM2DBL(v);
    v = rb_ary_entry(values, 2);
    am->ry = v == Qnil ? 0.0 : NUM2DBL(v);
    v = rb_ary_entry(values, 3);
    am->sy = v == Qnil ? 1.0 : NUM2DBL(v);
    v = rb_ary_entry(values, 4);
    am->tx = v == Qnil ? 0.0 : NUM2DBL(v);
    v = rb_ary_entry(values, 4);
    am->ty = v == Qnil ? 0.0 : NUM2DBL(v);
}


/*
    Extern:     ClassType_new
    Purpose:    Construct a ClassType enum object for the specified value
*/
VALUE
ClassType_new(ClassType cls)
{
    const char *name;

    switch(cls)
    {
        default:
        case UndefinedClass:
            name = "UndefineClass";
            break;
        case DirectClass:
            name = "DirectClass";
            break;
        case PseudoClass:
            name = "PseudoClass";
            break;
    }

    return rm_enum_new(Class_ClassType, ID2SYM(rb_intern(name)), INT2FIX(cls));
}


/*
    Extern:     ColorspaceType_new
    Purpose:    construct a ColorspaceType enum object for the specified value
*/
VALUE
ColorspaceType_new(ColorspaceType cs)
{
    const char *name;

    switch(cs)
    {
        default:
        case UndefinedColorspace:
            name = "UndefinedColorspace";
            break;
        case RGBColorspace:
            name = "RGBColorspace";
            break;
        case GRAYColorspace:
            name = "GRAYColorspace";
            break;
        case TransparentColorspace:
            name = "TransparentColorspace";
            break;
        case OHTAColorspace:
            name = "OHTAColorspace";
            break;
        case XYZColorspace:
            name = "XYZColorspace";
            break;
        case YCbCrColorspace:
            name = "YCbCrColorspace";
            break;
        case YCCColorspace:
            name = "YCCColorspace";
            break;
        case YIQColorspace:
            name = "YIQColorspace";
            break;
        case YPbPrColorspace:
            name = "YPbPrColorspace";
            break;
        case YUVColorspace:
            name = "YUVColorspace";
            break;
        case CMYKColorspace:
            name = "CMYKColorspace";
            break;
        case sRGBColorspace:
            name = "sRGBColorspace";
            break;
#if defined(HAVE_HSLCOLORSPACE)
        case HSLColorspace:
            name = "HSLColorspace";
            break;
#endif
#if defined(HAVE_HWBCOLORSPACE)
        case HWBColorspace:
            name = "HWBColorspace";
            break;
#endif
    }

    return rm_enum_new(Class_ColorspaceType, ID2SYM(rb_intern(name)), INT2FIX(cs));

}

/*
 *  Static:     ComplianceType_new
    Purpose:    construct a ComplianceType enum object for the specified value
*/
static VALUE
ComplianceType_new(ComplianceType compliance)
{
    const char *name;

    // Turn off undefined bits
    compliance &= (SVGCompliance|X11Compliance|XPMCompliance);
    name = Compliance_Const_Name(&compliance);
    return rm_enum_new(Class_ComplianceType, ID2SYM(rb_intern(name)), INT2FIX(compliance));
}

/*
 * External:    CompressionType_new
   Purpose:     Construct a CompressionTYpe enum object for the specified value
*/
VALUE
CompressionType_new(CompressionType ct)
{
    const char *name;

    switch (ct)
    {
        default:
        case UndefinedCompression:
            name = "UndefinedCompression";
            break;
        case NoCompression:
            name = "NoCompression";
            break;
        case BZipCompression:
            name = "BZipCompression";
            break;
        case FaxCompression:
            name = "FaxCompression";
            break;
        case Group4Compression:
            name = "Group4Compression";
            break;
        case JPEGCompression:
            name = "JPEGCompression";
            break;
        case LosslessJPEGCompression:
            name = "LosslessJPEGCompression";
            break;
        case LZWCompression:
            name = "LZWCompression";
            break;
        case RLECompression:
            name = "RLECompression";
            break;
        case ZipCompression:
            name = "ZipCompression";
            break;
    }

    return rm_enum_new(Class_CompressionType, ID2SYM(rb_intern(name)), INT2FIX(ct));
}

/*
    External:  FilterTypes#new
    Purpose: Construct an FilterTypes enum object for the specified value
*/
VALUE
FilterTypes_new(FilterTypes type)
{
    const char *name;

    switch(type)
    {
        default:
        case UndefinedFilter:
            name = "UndefinedFilter";
            break;
        case PointFilter:
            name = "PointFilter";
            break;
        case BoxFilter:
            name = "BoxFilter";
            break;
        case TriangleFilter:
            name = "TriangleFilter";
            break;
        case HermiteFilter:
            name = "HermiteFilter";
            break;
        case HanningFilter:
            name = "HanningFilter";
            break;
        case HammingFilter:
            name = "HammingFilter";
            break;
        case BlackmanFilter:
            name = "BlackmanFilter";
            break;
        case GaussianFilter:
            name = "GaussianFilter";
            break;
        case QuadraticFilter:
            name = "QuadraticFilter";
            break;
        case CubicFilter:
            name = "CubicFilter";
            break;
        case CatromFilter:
            name = "CatromFilter";
            break;
        case MitchellFilter:
            name = "MitchellFilter";
            break;
        case LanczosFilter:
            name = "LanczosFilter";
            break;
        case BesselFilter:
            name = "BesselFilter";
            break;
        case SincFilter:
            name = "SincFilter";
            break;

    }
    return rm_enum_new(Class_FilterTypes, ID2SYM(rb_intern(name)), INT2FIX(type));
}

/*
    External:  ImageType#new
    Purpose: Construct an ImageType enum object for the specified value
*/
VALUE
ImageType_new(ImageType type)
{
    const char *name;

    switch(type)
    {
        default:
        case UndefinedType:
            name = "UndefinedType";
            break;
        case BilevelType:
            name = "BilevelType";
            break;
        case GrayscaleType:
            name = "GrayscaleType";
            break;
        case GrayscaleMatteType:
            name = "GrayscaleMatteType";
            break;
        case PaletteType:
            name = "PaletteType";
            break;
        case PaletteMatteType:
            name = "PaletteMatteType";
            break;
        case TrueColorType:
            name = "TrueColorType";
            break;
        case TrueColorMatteType:
            name = "TrueColorMatteType";
            break;
        case ColorSeparationType:
            name = "ColorSeparationType";
            break;
        case ColorSeparationMatteType:
            name = "ColorSeparationMatteType";
            break;
        case OptimizeType:
            name = "OptimizeType";
            break;
    }

    return rm_enum_new(Class_ImageType, ID2SYM(rb_intern(name)), INT2FIX(type));
}

/*
    External:   InterlaceType_new
    Purpose:    Construct an InterlaceType enum object for the specified value.
*/
VALUE
InterlaceType_new(InterlaceType interlace)
{
    const char *name;

    switch(interlace)
    {
        default:
        case UndefinedInterlace:
            name = "UndefinedInterlace";
            break;
        case NoInterlace:
            name = "NoInterlace";
            break;
        case LineInterlace:
            name = "LineInterlace";
            break;
        case PlaneInterlace:
            name = "PlaneInterlace";
            break;
        case PartitionInterlace:
            name = "PartitionInterlace";
            break;
    }

    return rm_enum_new(Class_InterlaceType, ID2SYM(rb_intern(name)), INT2FIX(interlace));
}

/*
    External:   RenderingIntent_new
    Purpose:    Construct an RenderingIntent enum object for the specified value.
*/
VALUE
RenderingIntent_new(RenderingIntent intent)
{
    const char *name;

    switch(intent)
    {
        default:
        case UndefinedIntent:
            name = "UndefinedIntent";
            break;
        case SaturationIntent:
            name = "SaturationIntent";
            break;
        case PerceptualIntent:
            name = "PerceptualIntent";
            break;
        case AbsoluteIntent:
            name = "AbsoluteIntent";
            break;
        case RelativeIntent:
            name = "RelativeIntent";
            break;
    }

    return rm_enum_new(Class_RenderingIntent, ID2SYM(rb_intern(name)), INT2FIX(intent));
}

/*
    External:   ResolutionType_new
    Purpose:    Construct an ResolutionType enum object for the specified value.
*/
VALUE
ResolutionType_new(ResolutionType type)
{
    const char *name;

    switch(type)
    {
        default:
        case UndefinedResolution:
            name = "UndefinedResolution";
            break;
        case PixelsPerInchResolution:
            name = "PixelsPerInchResolution";
            break;
        case PixelsPerCentimeterResolution:
            name = "PixelsPerCentimeterResolution";
            break;
    }
    return rm_enum_new(Class_ResolutionType, ID2SYM(rb_intern(name)), INT2FIX(type));
}

/*
    External:   Color_from_ColorInfo
    Purpose:    Convert a ColorInfo structure to a Magick::Color
*/
VALUE
Color_from_ColorInfo(const ColorInfo *ci)
{
    ComplianceType compliance_type;
    volatile VALUE name;
    volatile VALUE compliance;
    volatile VALUE color;

    name       = rb_str_new2(ci->name);

    compliance_type = ci->compliance;
    compliance = ComplianceType_new(compliance_type);
    color      = Pixel_from_PixelPacket((PixelPacket *)(&(ci->color)));

    return rb_funcall(Class_Color, new_ID, 3
                    , name, compliance, color);
}


/*
    External:   Color_to_ColorInfo
    Purpose:    Convert a Magick::Color to a ColorInfo structure
*/
void
Color_to_ColorInfo(ColorInfo *ci, VALUE st)
{
    volatile VALUE members, m;

    if (CLASS_OF(st) != Class_Color)
    {
        rb_raise(rb_eTypeError, "type mismatch: %s given",
                 rb_class2name(CLASS_OF(st)));
    }

    memset(ci, '\0', sizeof(ColorInfo));

    members = rb_funcall(st, values_ID, 0);

    m = rb_ary_entry(members, 0);
    if (m != Qnil)
    {
        CloneString((char **)&(ci->name), STRING_PTR(m));
    }
    m = rb_ary_entry(members, 1);
    if (m != Qnil)
    {
        VALUE_TO_ENUM(m, ci->compliance, ComplianceType);
    }
    m = rb_ary_entry(members, 2);
    if (m != Qnil)
    {
        Pixel_to_PixelPacket(&(ci->color), m);
    }
}

/*
    Static:     destroy_ColorInfo
    Purpose:    free the storage allocated by Color_to_ColorInfo, above.
*/
static void
destroy_ColorInfo(ColorInfo *ci)
{
    magick_free((void*)ci->name);
    ci->name = NULL;
}

/*
    Method:     Color#to_s
    Purpose:    Return a string representation of a Magick::Color object
*/
VALUE
Color_to_s(VALUE self)
{
    ColorInfo ci;
    char buff[1024];

    Color_to_ColorInfo(&ci, self);
    sprintf(buff, "name=%s, compliance=%s, "
                  "color.red=%d, color.green=%d, color.blue=%d, color.opacity=%d ",
                  ci.name,
                  Compliance_Const_Name(&ci.compliance),
                  ci.color.red, ci.color.green, ci.color.blue, ci.color.opacity);

    destroy_ColorInfo(&ci);
    return rb_str_new2(buff);
}

/*
    Extern:     Pixel_from_PixelPacket
    Purpose:    Create a Magick::Pixel object from a PixelPacket structure.
*/
VALUE
Pixel_from_PixelPacket(PixelPacket *pp)
{
    return rb_funcall(Class_Pixel, new_ID, 4
                    , INT2FIX(pp->red), INT2FIX(pp->green)
                    , INT2FIX(pp->blue), INT2FIX(pp->opacity));
}

/*
    Extern:     Pixel_to_PixelPacket
    Purpose:    Convert a Magick::Pixel object to a PixelPacket structure.
    Notes:      The Pixel object could have uninitialized values, default to 0.
*/
void
Pixel_to_PixelPacket(PixelPacket *pp, VALUE st)
{
    volatile VALUE values;
    volatile VALUE c;

    if (CLASS_OF(st) != Class_Pixel)
    {
        rb_raise(rb_eTypeError, "type mismatch: %s given",
                 rb_class2name(CLASS_OF(st)));
    }

    values = rb_funcall(st, values_ID, 0);
    c = rb_ary_entry(values, 0);
    pp->red = c != Qnil ? NUM2INT(c) : 0;
    c = rb_ary_entry(values, 1);
    pp->green = c != Qnil ? NUM2INT(c) : 0;
    c = rb_ary_entry(values, 2);
    pp->blue = c != Qnil ? NUM2INT(c) : 0;
    c = rb_ary_entry(values, 3);
    pp->opacity = c != Qnil ? NUM2INT(c) : 0;
}

/*
    Extern:     Color_to_PixelPacket
    Purpose:    Convert either a String color name or
                a Magick::Pixel to a PixelPacket
*/
void
Color_to_PixelPacket(PixelPacket *pp, VALUE color)
{
    // Allow color name or Pixel
    if (TYPE(color) == T_STRING)
    {
        Color_Name_to_PixelPacket(pp, color);
    }
    else if (CLASS_OF(color) == Class_Pixel)
    {
        Pixel_to_PixelPacket(pp, color);
    }
    else
    {
        rb_raise(rb_eTypeError, "color argument must be String or Pixel (%s given)",
                rb_class2name(CLASS_OF(color)));
    }

}

/*
    Extern:     PrimaryInfo_from_PrimaryInfo(pp)
    Purpose:    Create a Magick::PrimaryInfo object from a PrimaryInfo structure.
*/
VALUE
PrimaryInfo_from_PrimaryInfo(PrimaryInfo *p)
{
    return rb_funcall(Class_Primary, new_ID, 3
                    , INT2FIX(p->x), INT2FIX(p->y), INT2FIX(p->z));
}

/*
    Extern:     PrimaryInfo_to_PrimaryInfo
    Purpose:    Convert a Magick::PrimaryInfo object to a PrimaryInfo structure
*/
void
PrimaryInfo_to_PrimaryInfo(PrimaryInfo *pi, VALUE sp)
{
    volatile VALUE members, m;

    if (CLASS_OF(sp) != Class_Primary)
    {
        rb_raise(rb_eTypeError, "type mismatch: %s given",
                 rb_class2name(CLASS_OF(sp)));
    }
    members = rb_funcall(sp, values_ID, 0);
    m = rb_ary_entry(members, 0);
    pi->x = m == Qnil ? 0 : FIX2INT(m);
    m = rb_ary_entry(members, 1);
    pi->y = m == Qnil ? 0 : FIX2INT(m);
    m = rb_ary_entry(members, 2);
    pi->z = m == Qnil ? 0 : FIX2INT(m);
}

/*
    Extern:     PointInfo_to_Point(pp)
    Purpose:    Create a Magick::Point object from a PointInfo structure.
*/
VALUE
PointInfo_to_Point(PointInfo *p)
{
    return rb_funcall(Class_Point, new_ID, 2
                    , INT2FIX(p->x), INT2FIX(p->y));
}

/*
    Extern:     Point_to_PointInfo
    Purpose:    Convert a Magick::Point object to a PointInfo structure
*/
void
Point_to_PointInfo(PointInfo *pi, VALUE sp)
{
    volatile VALUE members, m;

    if (CLASS_OF(sp) != Class_Point)
    {
        rb_raise(rb_eTypeError, "type mismatch: %s given",
                 rb_class2name(CLASS_OF(sp)));
    }
    members = rb_funcall(sp, values_ID, 0);
    m = rb_ary_entry(members, 0);
    pi->x = m == Qnil ? 0 : FIX2INT(m);
    m = rb_ary_entry(members, 1);
    pi->y = m == Qnil ? 0 : FIX2INT(m);
}

/*
    Extern:     ChromaticityInfo_new(pp)
    Purpose:    Create a Magick::ChromaticityInfo object from a
                ChromaticityInfo structure.
*/
VALUE
ChromaticityInfo_new(ChromaticityInfo *ci)
{
    volatile VALUE red_primary;
    volatile VALUE green_primary;
    volatile VALUE blue_primary;
    volatile VALUE white_point;

    red_primary   = PrimaryInfo_from_PrimaryInfo(&ci->red_primary);
    green_primary = PrimaryInfo_from_PrimaryInfo(&ci->green_primary);
    blue_primary  = PrimaryInfo_from_PrimaryInfo(&ci->blue_primary);
    white_point   = PrimaryInfo_from_PrimaryInfo(&ci->white_point);

    return rb_funcall(Class_Chromaticity, new_ID, 4
                    , red_primary, green_primary, blue_primary, white_point);
}

/*
    Extern:     ChromaticityInfo_to_ChromaticityInfo
    Purpose:    Extract the elements from a Magick::ChromaticityInfo
                and store in a ChromaticityInfo structure.
*/
void
ChromaticityInfo_to_ChromaticityInfo(ChromaticityInfo *ci, VALUE chrom)
{
    volatile VALUE chrom_members;
    volatile VALUE red_primary, green_primary, blue_primary, white_point;
    volatile VALUE entry_members, x, y;
    ID values_id;

    if (CLASS_OF(chrom) != Class_Chromaticity)
    {
        rb_raise(rb_eTypeError, "type mismatch: %s given",
                 rb_class2name(CLASS_OF(chrom)));
    }
    values_id = values_ID;

    // Get the struct members in an array
    chrom_members = rb_funcall(chrom, values_id, 0);
    red_primary   = rb_ary_entry(chrom_members, 0);
    green_primary = rb_ary_entry(chrom_members, 1);
    blue_primary  = rb_ary_entry(chrom_members, 2);
    white_point = rb_ary_entry(chrom_members, 3);

    // Get the red_primary PrimaryInfo members in an array
    entry_members = rb_funcall(red_primary, values_id, 0);
    x = rb_ary_entry(entry_members, 0);         // red_primary.x
    ci->red_primary.x = x == Qnil ? 0.0 : NUM2DBL(x);
    y = rb_ary_entry(entry_members, 1);         // red_primary.y
    ci->red_primary.y = y == Qnil ? 0.0 : NUM2DBL(y);
    ci->red_primary.z = 0.0;

    // Get the green_primary PrimaryInfo members in an array
    entry_members = rb_funcall(green_primary, values_id, 0);
    x = rb_ary_entry(entry_members, 0);         // green_primary.x
    ci->green_primary.x = x == Qnil ? 0.0 : NUM2DBL(x);
    y = rb_ary_entry(entry_members, 1);         // green_primary.y
    ci->green_primary.y = y == Qnil ? 0.0 : NUM2DBL(y);
    ci->green_primary.z = 0.0;

    // Get the blue_primary PrimaryInfo members in an array
    entry_members = rb_funcall(blue_primary, values_id, 0);
    x = rb_ary_entry(entry_members, 0);         // blue_primary.x
    ci->blue_primary.x = x == Qnil ? 0.0 : NUM2DBL(x);
    y = rb_ary_entry(entry_members, 1);         // blue_primary.y
    ci->blue_primary.y = y == Qnil ? 0.0 : NUM2DBL(y);
    ci->blue_primary.z = 0.0;

    // Get the white_point PrimaryInfo members in an array
    entry_members = rb_funcall(white_point, values_id, 0);
    x = rb_ary_entry(entry_members, 0);         // white_point.x
    ci->white_point.x = x == Qnil ? 0.0 : NUM2DBL(x);
    y = rb_ary_entry(entry_members, 1);         // white_point.y
    ci->white_point.y = y == Qnil ? 0.0 : NUM2DBL(y);
    ci->white_point.z = 0.0;
}

/*
    External:   Rectangle_from_RectangleInfo
    Purpose:    Convert a RectangleInfo structure to a Magick::Rectangle
*/
VALUE
Rectangle_from_RectangleInfo(RectangleInfo *rect)
{
    volatile VALUE width;
    volatile VALUE height;
    volatile VALUE x, y;

    width  = UINT2NUM(rect->width);
    height = UINT2NUM(rect->height);
    x      = INT2NUM(rect->x);
    y      = INT2NUM(rect->y);
    return rb_funcall(Class_Rectangle, new_ID, 4
                    , width, height, x, y);
}

/*
    External:   Rectangle_to_RectangleInfo
    Purpose:    Convert a Magick::Rectangle to a RectangleInfo structure.
*/
void
Rectangle_to_RectangleInfo(RectangleInfo *rect, VALUE sr)
{
    volatile VALUE members, m;

    if (CLASS_OF(sr) != Class_Rectangle)
    {
        rb_raise(rb_eTypeError, "type mismatch: %s given",
                 rb_class2name(CLASS_OF(sr)));
    }
    members = rb_funcall(sr, values_ID, 0);
    m = rb_ary_entry(members, 0);
    rect->width  = m == Qnil ? 0 : NUM2ULONG(m);
    m = rb_ary_entry(members, 1);
    rect->height = m == Qnil ? 0 : NUM2ULONG(m);
    m = rb_ary_entry(members, 2);
    rect->x      = m == Qnil ? 0 : NUM2LONG (m);
    m = rb_ary_entry(members, 3);
    rect->y      = m == Qnil ? 0 : NUM2LONG (m);
}

/*
    External:   Segment_from_SegmentInfo
    Purpose:    Convert a SegmentInfo structure to a Magick::Segment
*/
VALUE
Segment_from_SegmentInfo(SegmentInfo *segment)
{
    volatile VALUE x1, y1, x2, y2;

    x1 = rb_float_new(segment->x1);
    y1 = rb_float_new(segment->y1);
    x2 = rb_float_new(segment->x2);
    y2 = rb_float_new(segment->y2);
    return rb_funcall(Class_Segment, new_ID, 4, x1, y1, x2, y2);
}

/*
    External:   Segment_to_SegmentInfo
    Purpose:    Convert a Magick::Segment to a SegmentInfo structure.
*/
void
Segment_to_SegmentInfo(SegmentInfo *segment, VALUE s)
{
    volatile VALUE members, m;

    if (CLASS_OF(s) != Class_Segment)
    {
        rb_raise(rb_eTypeError, "type mismatch: %s given",
                 rb_class2name(CLASS_OF(s)));
    }

    members = rb_funcall(s, values_ID, 0);
    m = rb_ary_entry(members, 0);
    segment->x1 = m == Qnil ? 0.0 : NUM2DBL(m);
    m = rb_ary_entry(members, 1);
    segment->y1 = m == Qnil ? 0.0 : NUM2DBL(m);
    m = rb_ary_entry(members, 2);
    segment->x2 = m == Qnil ? 0.0 : NUM2DBL(m);
    m = rb_ary_entry(members, 3);
    segment->y2 = m == Qnil ? 0.0 : NUM2DBL(m);
}

/*
    Static:     StretchType_new
    Purpose:    Construct a StretchType enum for a specified StretchType value
*/
static VALUE
StretchType_new(StretchType stretch)
{
    const char *name;

    name = StretchType_Const_Name(stretch);
    return rm_enum_new(Class_StretchType, ID2SYM(rb_intern(name)), INT2FIX(stretch));
}


/*
    Static:     StyleType_new
    Purpose:    Construct a StyleType enum for a specified StyleType value
*/
static VALUE
StyleType_new(StyleType style)
{
    const char *name;

    name = StyleType_Const_Name(style);
    return rm_enum_new(Class_StyleType, ID2SYM(rb_intern(name)), INT2FIX(style));
}

/*
    External:   Font_from_TypeInfo
    Purpose:    Convert a TypeInfo structure to a Magick::Font
*/
VALUE
Font_from_TypeInfo(TypeInfo *ti)
{
    volatile VALUE name, description, family;
    volatile VALUE style, stretch, weight;
    volatile VALUE encoding, foundry, format;

    name        = rb_str_new2(ti->name);
    description = rb_str_new2(ti->description);
    family      = rb_str_new2(ti->family);
    style       = StyleType_new(ti->style);
    stretch     = StretchType_new(ti->stretch);
    weight      = INT2NUM(ti->weight);
    encoding    = ti->encoding ? rb_str_new2(ti->encoding) : Qnil;
    foundry     = ti->foundry  ? rb_str_new2(ti->foundry)  : Qnil;
    format      = ti->format   ? rb_str_new2(ti->format)   : Qnil;

    return rb_funcall(Class_Font, new_ID, 9
                    , name, description, family, style
                    , stretch, weight, encoding, foundry, format);
}

/*
    External:   Font_to_TypeInfo
    Purpose:    Convert a Magick::Font to a TypeInfo structure
*/
void
Font_to_TypeInfo(TypeInfo *ti, VALUE st)
{
    volatile VALUE members, m;

    if (CLASS_OF(st) != Class_Font)
    {
        rb_raise(rb_eTypeError, "type mismatch: %s given",
                 rb_class2name(CLASS_OF(st)));
    }

    memset(ti, '\0', sizeof(TypeInfo));

    members = rb_funcall(st, values_ID, 0);
    m = rb_ary_entry(members, 0);
    if (m != Qnil)
    {
        CloneString((char **)&(ti->name), STRING_PTR(m));
    }
    m = rb_ary_entry(members, 1);
    if (m != Qnil)
    {
        CloneString((char **)&(ti->description), STRING_PTR(m));
    }
    m = rb_ary_entry(members, 2);
    if (m != Qnil)
    {
        CloneString((char **)&(ti->family), STRING_PTR(m));
    }
    m = rb_ary_entry(members, 3); ti->style   = m == Qnil ? 0 : FIX2INT(m);
    m = rb_ary_entry(members, 4); ti->stretch = m == Qnil ? 0 : FIX2INT(m);
    m = rb_ary_entry(members, 5); ti->weight  = m == Qnil ? 0 : FIX2INT(m);

    m = rb_ary_entry(members, 6);
    if (m != Qnil)
        CloneString((char **)&(ti->encoding), STRING_PTR(m));
    m = rb_ary_entry(members, 7);
    if (m != Qnil)
        CloneString((char **)&(ti->foundry), STRING_PTR(m));
    m = rb_ary_entry(members, 8);
    if (m != Qnil)
        CloneString((char **)&(ti->format), STRING_PTR(m));
}

/*
    Static:     destroy_TypeInfo
    Purpose:    free the storage allocated by Font_to_TypeInfo, above.
*/
static void
destroy_TypeInfo(TypeInfo *ti)
{
    magick_free((void*)ti->name);
    ti->name = NULL;
    magick_free((void*)ti->description);
    ti->description = NULL;
    magick_free((void*)ti->family);
    ti->family = NULL;
    magick_free((void*)ti->encoding);
    ti->encoding = NULL;
    magick_free((void*)ti->foundry);
    ti->foundry = NULL;
    magick_free((void*)ti->format);
    ti->format = NULL;
}

/*
    External:   Font_to_s
    Purpose:    implement the Font#to_s method
*/
VALUE
Font_to_s(VALUE self)
{
    TypeInfo ti;
    char weight[20];
    char buff[1024];

    Font_to_TypeInfo(&ti, self);

    switch (ti.weight)
    {
        case 400:
            strcpy(weight, "NormalWeight");
            break;
        case 700:
            strcpy(weight, "BoldWeight");
            break;
        default:
            sprintf(weight, "%lu", ti.weight);
            break;
    }

    sprintf(buff, "name=%s, description=%s, "
                  "family=%s, style=%s, stretch=%s, weight=%s, "
                  "encoding=%s, foundry=%s, format=%s",
                  ti.name,
                  ti.description,
                  ti.family,
                  StyleType_Const_Name(ti.style),
                  StretchType_Const_Name(ti.stretch),
                  weight,
                  ti.encoding ? ti.encoding : "",
                  ti.foundry ? ti.foundry : "",
                  ti.format ? ti.format : "");

    destroy_TypeInfo(&ti);
    return rb_str_new2(buff);

}

/*
    External:   TypeMetric_from_TypeMetric
    Purpose:    Convert a TypeMetric structure to a Magick::TypeMetric
*/
VALUE
TypeMetric_from_TypeMetric(TypeMetric *tm)
{
    volatile VALUE pixels_per_em;
    volatile VALUE ascent, descent;
    volatile VALUE width, height, max_advance;
    volatile VALUE bounds, underline_position, underline_thickness;

    pixels_per_em       = PointInfo_to_Point(&tm->pixels_per_em);
    ascent              = rb_float_new(tm->ascent);
    descent             = rb_float_new(tm->descent);
    width               = rb_float_new(tm->width);
    height              = rb_float_new(tm->height);
    max_advance         = rb_float_new(tm->max_advance);
    bounds              = Segment_from_SegmentInfo(&tm->bounds);
    underline_position  = rb_float_new(tm->underline_position);
    underline_thickness = rb_float_new(tm->underline_position);

    return rb_funcall(Class_TypeMetric, new_ID, 9
                    , pixels_per_em, ascent, descent, width
                    , height, max_advance, bounds
                    , underline_position, underline_thickness);
}

/*
    External:   TypeMetric_to_TypeMetric
    Purpose:    Convert a Magick::TypeMetric to a TypeMetric structure.
*/
void
TypeMetric_to_TypeMetric(TypeMetric *tm, VALUE st)
{
    volatile VALUE members, m;
    volatile VALUE pixels_per_em;

    if (CLASS_OF(st) != Class_TypeMetric)
    {
        rb_raise(rb_eTypeError, "type mismatch: %s given",
                 rb_class2name(CLASS_OF(st)));
    }
    members = rb_funcall(st, values_ID, 0);

    pixels_per_em   = rb_ary_entry(members, 0);
    Point_to_PointInfo(&tm->pixels_per_em, pixels_per_em);

    m = rb_ary_entry(members, 1);
    tm->ascent      = m == Qnil ? 0.0 : NUM2DBL(m);
    m = rb_ary_entry(members, 2);
    tm->descent     = m == Qnil ? 0.0 : NUM2DBL(m);
    m = rb_ary_entry(members, 3);
    tm->width       = m == Qnil ? 0.0 : NUM2DBL(m);
    m = rb_ary_entry(members, 4);
    tm->height      = m == Qnil ? 0.0 : NUM2DBL(m);
    m = rb_ary_entry(members, 5);
    tm->max_advance = m == Qnil ? 0.0 : NUM2DBL(m);

    m = rb_ary_entry(members, 6);
    Segment_to_SegmentInfo(&tm->bounds, m);

    m = rb_ary_entry(members, 7);
    tm->underline_position  = m == Qnil ? 0.0 : NUM2DBL(m);
    m = rb_ary_entry(members, 8);
    tm->underline_thickness = m == Qnil ? 0.0 : NUM2DBL(m);
}

/*
    Method:     Magick::TypeMetric#to_s
    Purpose:    Create a string representation of a Magick::TypeMetric
*/
VALUE
TypeMetric_to_s(VALUE self)
{
    TypeMetric tm;
    char buff[200];

    TypeMetric_to_TypeMetric(&tm, self);
    sprintf(buff, "pixels_per_em=(x=%g,y=%g) "
                  "ascent=%g descent=%g width=%g height=%g max_advance=%g "
                  "bounds.x1=%g bounds.y1=%g bounds.x2=%g bounds.y2=%g "
                  "underline_position=%g underline_thickness=%g",
                  tm.pixels_per_em.x, tm.pixels_per_em.y,
                  tm.ascent, tm.descent, tm.width, tm.height, tm.max_advance,
                  tm.bounds.x1, tm.bounds.y1, tm.bounds.x2, tm.bounds.y2,
                  tm.underline_position, tm.underline_thickness);
    return rb_str_new2(buff);
}

#if defined(HAVE_RB_DEFINE_ALLOC_FUNC)
/*
    Extern:  rm_enum_new (1.8)
    Purpose: Construct a new Enum instance
*/
VALUE rm_enum_new(VALUE class, VALUE sym, VALUE val)
{
   return Enum_initialize(Enum_alloc(class), sym, val);
}

/*
    Extern:  Enum_alloc (1.8)
    Purpose: Enum class alloc function
*/
VALUE Enum_alloc(VALUE class)
{
   MagickEnum *magick_enum;

   return Data_Make_Struct(class, MagickEnum, NULL, NULL, magick_enum);
}
#else
/*
    Extern:  rm_enum_new (1.6)
    Purpose: Construct a new Enum instance
*/
VALUE rm_enum_new(VALUE class, VALUE sym, VALUE val)
{
    return Enum_new(class, sym, val);
}

/*
    Method:  Enum.new
    Purpose: Construct a new Enum object
    Notes:   `class' can be an Enum subclass
*/
VALUE Enum_new(VALUE class, VALUE sym, VALUE val)
{
    volatile VALUE new_enum;
    VALUE argv[2];
    MagickEnum *magick_enum;

    new_enum = Data_Make_Struct(class, MagickEnum, NULL, NULL, magick_enum);
    argv[0] = sym;
    argv[1] = val;

    rb_obj_call_init(new_enum, 2, argv);
    return new_enum;
}
#endif

/*
    Method:  Enum#initialize
    Purpose: Initialize a new Enum instance
*/
VALUE Enum_initialize(VALUE self, VALUE sym, VALUE val)
{
   MagickEnum *magick_enum;

   Data_Get_Struct(self, MagickEnum, magick_enum);
   magick_enum->id = rb_to_id(sym); /* convert symbol to ID */
   magick_enum->val = NUM2INT(val);

   return self;
}

/*
    Method:  Enum#to_s
    Purpose: Return the name of an enum
*/
VALUE Enum_to_s(VALUE self)
{
   MagickEnum *magick_enum;

   Data_Get_Struct(self, MagickEnum, magick_enum);
   return rb_str_new2(rb_id2name(magick_enum->id));
}

/*
    Method:  Enum#to_i
    Purpose: Return the value of an enum
*/
VALUE Enum_to_i(VALUE self)
{
   MagickEnum *magick_enum;

   Data_Get_Struct(self, MagickEnum, magick_enum);
   return INT2NUM(magick_enum->val);
}

/*
    Method:  Enum#<=>
    Purpose: Support Enumerable module in Enum
    Returns: -1, 0, 1, or nil
    Notes:   Enums must be instances of the same class to be equal.
*/
VALUE Enum_spaceship(VALUE self, VALUE other)
{
    MagickEnum *this, *that;

    Data_Get_Struct(self, MagickEnum, this);
    Data_Get_Struct(other, MagickEnum, that);

    if (this->val > that->val)
    {
        return INT2FIX(1);
    }
    else if (this->val < that->val)
    {
        return INT2FIX(-1);
    }

    // Values are equal, check class.

    return rb_funcall(CLASS_OF(self), rb_intern("<=>"), 1, CLASS_OF(other));
}

/*
    Method:  Enum#===
    Purpose: "Case equal" operator for Enum
    Returns: true or false
    Notes:   Yes, I know "case equal" is a misnomer.
*/
VALUE Enum_case_eq(VALUE self, VALUE other)
{
    MagickEnum *this, *that;

    if (CLASS_OF(self) == CLASS_OF(other))
    {
        Data_Get_Struct(self, MagickEnum, this);
        Data_Get_Struct(other, MagickEnum, that);
        return this->val == that->val ? Qtrue : Qfalse;
    }

    return Qfalse;
}


/*
    Static:     Compliance_Const_Name
    Purpose:    Return the string representation of a ComplianceType value
    Notes:      xMagick will OR multiple compliance types so we have to
                arbitrarily pick one name. Set the compliance argument
                to the selected value.
*/
static const char *
Compliance_Const_Name(ComplianceType *c)
{
    if ((*c & (SVGCompliance|X11Compliance|XPMCompliance))
        == (SVGCompliance|X11Compliance|XPMCompliance))
    {
        return "AllCompliance";
    }
    else if (*c & SVGCompliance)
    {
        *c = SVGCompliance;
        return "SVGCompliance";
    }
    else if (*c & X11Compliance)
    {
        *c = X11Compliance;
        return "X11Compliance";
    }
    else if (*c & XPMCompliance)
    {
        *c = XPMCompliance;
        return "XPMCompliance";
    }
#if defined(HAVE_NOCOMPLIANCE)
    else if (*c != NoCompliance)
    {
        return "unknown";
    }
    else
    {
        *c = NoCompliance;
        return "NoCompliance";
    }
#else
    else if (*c != UnknownCompliance)
    {
        return "unknown";
    }
    else
    {
        *c = UnknownCompliance;
        return "UnknownCompliance";
    }
#endif
}

/*
    Static:     StretchType_Const_Name
    Purpose:    Return the string representation of a StretchType value
*/
static const char *
StretchType_Const_Name(StretchType stretch)
{
    switch (stretch)
    {
        case NormalStretch:
            return "NormalStretch";
        case UltraCondensedStretch:
            return "UltraCondensedStretch";
        case ExtraCondensedStretch:
            return "ExtraCondensedStretch";
        case CondensedStretch:
            return "CondensedStretch";
        case SemiCondensedStretch:
            return "SemiCondensedStretch";
        case SemiExpandedStretch:
            return "SemiExpandedStretch";
        case ExpandedStretch:
            return "ExpandedStretch";
        case ExtraExpandedStretch:
            return "ExtraExpandedStretch";
        case UltraExpandedStretch:
            return "UltraExpandedStretch";
        case AnyStretch:
            return "AnyStretch";
        default:
            return "unknown";
    }
}


/*
    Static:     StyleType_Const_Name
    Purpose:    Return the string representation of a StyleType value
*/
static const char *
StyleType_Const_Name(StyleType style)
{
    switch (style)
    {
        case NormalStyle:
            return "NormalStyle";
        case ItalicStyle:
            return "ItalicStyle";
        case ObliqueStyle:
            return "ObliqueStyle";
        case AnyStyle:
            return "AnyStyle";
        default:
            return "unknown";
    }
}


/*
    External:   write_temp_image
    Purpose:    Write a temporary copy of the image to the IM registry
    Returns:    the "filename" of the registered image
    Notes:      The `tmpnam' argument must point to an char array
                of size MaxTextExtent.
*/
void
write_temp_image(Image *image, char *tmpnam)
{
    long registry_id;

    registry_id = SetMagickRegistry(ImageRegistryType, image, sizeof(Image), &image->exception);
    if (registry_id < 0)
    {
        rb_raise(rb_eRuntimeError, "SetMagickRegistry failed.");
    }
    HANDLE_IMG_ERROR(image)

    sprintf(tmpnam, "mpri:%ld", registry_id);
}

/*
    External:   delete_temp_image
    Purpose:    Delete the temporary image from the registry
    Returns:    void
*/

void
delete_temp_image(char *tmpnam)
{
    long registry_id = -1;

    sscanf(tmpnam, "mpri:%ld", &registry_id);
    if (registry_id >= 0)
    {
        (void) DeleteMagickRegistry(registry_id);
    }
}

/*
    External:   not_implemented
    Purpose:    raise NotImplementedError
    Notes:      Called when a xMagick API is not available.
                Replaces Ruby's rb_notimplement function.
    Notes:      The MagickPackageName macro is not available
                until 5.5.7. Use MAGICKNAME instead.
*/
void
not_implemented(const char *method)
{

    rb_raise(rb_eNotImpError, "the %s method is not supported by "
                              Q(MAGICKNAME) " " MagickLibVersionText
                              , method);
}

/*
    Static:     raise_error(msg, loc)
    Purpose:    create a new ImageMagickError object and raise an exception
    Notes:      does not return
                This funky technique allows me to safely add additional
                information to the ImageMagickError object in both 1.6.8 and
                1.8.0. See www.ruby_talk.org/36408.
*/
static void
raise_error(const char *msg, const char *loc)
{
    volatile VALUE exc, mesg, extra;

    mesg = rb_str_new2(msg);
    extra = loc ? rb_str_new2(loc) : Qnil;

    exc = rb_funcall(Class_ImageMagickError, new_ID, 2, mesg, extra);
    rb_funcall(rb_cObject, rb_intern("raise"), 1, exc);
}


/*
    Method:     ImageMagickError#initialize(msg, loc)
    Purpose:    initialize a new ImageMagickError object - store
                the "loc" string in the @magick_location instance variable
*/
VALUE
ImageMagickError_initialize(VALUE self, VALUE mesg, VALUE extra)
{
    volatile VALUE argv[1];

    argv[0] = mesg;
    rb_call_super(1, (VALUE *)argv);
    rb_iv_set(self, "@"MAGICK_LOC, extra);

    return self;
}


/*
    Static:     magick_error_handler
    Purpose:    Build error or warning message string. If the error
                is severe, raise the ImageMagickError exception,
                otherwise print an optional warning.
*/
static void
magick_error_handler(
    ExceptionType severity,
    const char *reason,
    const char *description
#if defined(HAVE_EXCEPTIONINFO_MODULE)
    ,
    const char *module,
    const char *function,
    unsigned long line
#endif
    )
{
    char msg[1024];

    if (severity > WarningException)
    {
#if defined(HAVE_SNPRINTF)
        snprintf(msg, sizeof(msg)-1,
#else
        sprintf(msg,
#endif
                     "%s%s%s",
            GET_MSG(severity, reason),
            description ? ": " : "",
            description ? GET_MSG(severity, description) : "");

#if defined(HAVE_EXCEPTIONINFO_MODULE)
        {
        char extra[100];

#if defined(HAVE_SNPRINTF)
        snprintf(extra, sizeof(extra)-1, "%s at %s:%lu", function, module, line);
#else
        sprintf(extra, "%s at %s:%lu", function, module, line);
#endif
        raise_error(msg, extra);
        }
#else
        raise_error(msg, NULL);
#endif
    }
    else if (severity != UndefinedException)
    {
#if defined(HAVE_SNPRINTF)
        snprintf(msg, sizeof(msg)-1,
#else
        sprintf(msg,
#endif
                     "RMagick: %s%s%s",
            GET_MSG(severity, reason),
            description ? ": " : "",
            description ? GET_MSG(severity, description) : "");
        rb_warning(msg);
    }
}


/*
    Extern:     handle_error
    Purpose:    Called from RMagick routines to issue warning messages
                and raise the ImageMagickError exception.
    Notes:      In order to free up memory before calling raise, this
                routine copies the ExceptionInfo data to local storage
                and then calls DestroyExceptionInfo before raising
                the error.

                If the exception is an error, DOES NOT RETURN!
*/
void
handle_error(ExceptionInfo *ex)
{
    ExceptionType sev = ex->severity;
    char reason[251];
    char desc[251];

#if defined(HAVE_EXCEPTIONINFO_MODULE)
    char module[251], function[251];
    unsigned long line;
#endif

    reason[0] = '\0';
    desc[0] = '\0';

    if (sev == UndefinedException)
    {
        return;
    }
    if (ex->reason)
    {
        strncpy(reason, ex->reason, 250);
        reason[250] = '\0';
    }
    if (ex->description)
    {
        strncpy(desc, ex->description, 250);
        desc[250] = '\0';
    }

#if defined(HAVE_EXCEPTIONINFO_MODULE)
    module[0] = '\0';
    function[0] = '\0';

    if (ex->module)
    {
        strncpy(module, ex->module, 250);
        module[250] = '\0';
    }
    if (ex->function)
    {
        strncpy(function, ex->function, 250);
        function[250] = '\0';
    }
    line = ex->line;
#endif

    // Let ImageMagick reclaim its storage
    DestroyExceptionInfo(ex);
    // Reset the severity. If the exception structure is in an
    // Image and this exception is rescued and the Image reused,
    // we need the Image to be pristine!
    GetExceptionInfo(ex);

#if !defined(HAVE_EXCEPTIONINFO_MODULE)
    magick_error_handler(sev, reason, desc);
#else
    magick_error_handler(sev, reason, desc, module, function, line);
#endif
}

/*
    Extern:     handle_all_errors
    Purpose:    Examine all the images in a sequence. If any
                image has an error, raise an exception. Otherwise
                if any image has a warning, issue a warning message.
*/
void handle_all_errors(Image *seq)
{
    Image *badboy = NULL;
    Image *image = seq;

    while (image)
    {
        if (image->exception.severity != UndefinedException)
        {
            // Stop at the 1st image with an error
            if (image->exception.severity > WarningException)
            {
                badboy = image;
                break;
            }
            else if (!badboy)
            {
                badboy = image;
            }
        }
        image = GET_NEXT_IMAGE(image);
    }

    if (badboy)
    {
        if (badboy->exception.severity > WarningException)
        {
            unseq(seq);
        }
        handle_error(&badboy->exception);
    }
}



/*
    Extern:     toseq
    Purpose:    Convert an array of Image *s to an ImageMagick scene
                sequence (i.e. a doubly-linked list of Images)
    Returns:    a pointer to the head of the scene sequence list
*/
Image *
toseq(VALUE imagelist)
{
    long x, len;
    Image *head = NULL;
#if !defined(HAVE_APPENDIMAGETOLIST)
    Image *tail = NULL;
#endif

    Check_Type(imagelist, T_ARRAY);
    len = rm_imagelist_length(imagelist);
    if (len == 0)
    {
        rb_raise(rb_eArgError, "no images in this image list");
    }

    for (x = 0; x < len; x++)
    {
        Image *image;

        Data_Get_Struct(rb_ary_entry(imagelist, x), Image, image);
#if defined(HAVE_APPENDIMAGETOLIST)
        AppendImageToList(&head, image);
#else
        if (!head)
        {
            head = image;
        }
        else
        {
            image->previous = tail;
            tail->next = image;
        }
        tail = image;
#endif
    }

    return head;
}

/*
    Extern:     unseq
    Purpose:    Remove the ImageMagick links between images in an scene
                sequence.
    Notes:      The images remain grouped via the ImageList
*/
void
unseq(Image *image)
{

    if (!image)
    {
        rb_bug("RMagick FATAL: unseq called with NULL argument.");
    }
    while (image)
    {
#if HAVE_REMOVEFIRSTIMAGEFROMLIST
        (void) RemoveFirstImageFromList(&image);
#else
        Image *next;

        next = GET_NEXT_IMAGE(image);
        image->previous = image->next = NULL;
        image = next;
#endif
    }
}
