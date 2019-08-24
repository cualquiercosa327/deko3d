#include "../dk_device.h"
#include "../dk_queue.h"
#include "../cmdbuf_writer.h"

#include "mme_macros.h"
#include "engine_3d.h"

#include "../driver_constbuf.h"
#include "texture_image_control_block.h"

using namespace maxwell;
using namespace dk::detail;

using E = Engine3D;
using SRC = E::MmeShadowRamControl;

namespace
{
	constexpr auto SetShadowRamControl(unsigned mode)
	{
		return CmdInline(3D, MmeShadowRamControl{}, mode);
	}

	template <typename T>
	constexpr auto MacroFillRegisters(unsigned base, unsigned increment, unsigned count, T value)
	{
		return Macro(FillRegisters, base | (increment << 12), count, value);
	}

	template <typename Array, typename T>
	constexpr auto MacroFillArray(T value)
	{
		return MacroFillRegisters(Array{}, 1U, Array::Size, value);
	}

	template <typename Array, typename T>
	constexpr auto MacroFillRegisterArray(T value)
	{
		return MacroFillRegisters(Array{}, 1U, Array::Count << Array::Shift, value);
	}

	template <typename Array, typename T>
	constexpr auto MacroSetRegisterInArray(unsigned offset, T value)
	{
		return MacroFillRegisters(Array{} + offset, 1U << Array::Shift, Array::Count, value);
	}
}

void tag_DkQueue::setup3DEngine()
{
	CmdBufWriter w{&m_cmdBuf};

	w << CmdInline(3D, MultisampleEnable{}, 0);
	w << CmdInline(3D, CsaaEnable{}, 0);
	w << CmdInline(3D, MultisampleMode{}, MsaaMode_1x1);
	w << CmdInline(3D, MultisampleControl{}, 0);
	w << CmdInline(3D, Unknown1d3{}, 0x3F);
	w << MacroFillRegisterArray<E::WindowRectangle>(0);
	w << CmdInline(3D, ClearBufferFlags{}, E::ClearBufferFlags::StencilMask{} | E::ClearBufferFlags::Scissor{});
	w << MacroSetRegisterInArray<E::Scissor>(E::Scissor::Enable{}, 0);
	w << Cmd(3D, Unknown5aa{}, 0x00030003);
	w << Cmd(3D, Unknown5e5{}, 0x00020002);
	w << CmdInline(3D, PrimitiveRestartWithDrawArrays{}, 0);
	w << CmdInline(3D, PointRasterRules{}, 0);
	w << CmdInline(3D, LinkedTsc{}, 0);
	w << CmdInline(3D, ProvokingVertexLast{}, 1);
	w << CmdInline(3D, SetShaderExceptions{}, 0);
	w << CmdInline(3D, Unknown400{}, 0x10);
	w << CmdInline(3D, Unknown086{}, 0x10);
	w << CmdInline(3D, Unknown43f{}, 0x10);
	w << CmdInline(3D, Unknown4a4{}, 0x10);
	w << CmdInline(3D, Unknown4b6{}+0, 0x10);
	w << CmdInline(3D, Unknown4b6{}+1, 0x10);
	w << CmdInline(3D, SetApiVisibleCallLimit{}, E::SetApiVisibleCallLimit::Cta::_128);
	w << CmdInline(3D, Unknown450{}, 0x10);
	w << CmdInline(3D, Unknown584{}, 0x0E);
	w << MacroFillArray<E::IsVertexArrayPerInstance>(0);
	w << CmdInline(3D, VertexIdConfig{}, E::VertexIdConfig::AddVertexBase{});
	w << CmdInline(3D, ZcullStatCountersEnable{}, 1);
	w << CmdInline(3D, LineWidthSeparate{}, 1);
	w << CmdInline(3D, Unknown0c3{}, 0);
	w << CmdInline(3D, Unknown0c0{}, 3);
	w << CmdInline(3D, Unknown3f7{}, 1);
	w << CmdInline(3D, Unknown670{}, 1);
	w << CmdInline(3D, Unknown3e3{}, 0);
	w << CmdInline(3D, StencilTwoSideEnable{}, 1);
	w << CmdInline(3D, SetBindlessTexture{}, 0); // Using constbuf0 as the texture constbuf
	w << CmdInline(3D, SetSpaVersion{},
		E::SetSpaVersion::Major{5} | E::SetSpaVersion::Minor{3} // SM 5.3
	);
	w << Cmd(3D, SetShaderLocalMemoryWindow{}, 0x01000000);
	w << CmdInline(3D, Unknown44c{}, 0x13);
	w << CmdInline(3D, Unknown0dd{}, 0x00);
	w << Cmd(3D, SetRenderLayer{}, E::SetRenderLayer::UseIndexFromVTG{});
	w << CmdInline(3D, Unknown488{}, 5);
	w << Cmd(3D, Unknown514{}, 8 | (getDevice()->getGpuInfo().numWarpsPerSm << 16));
	// here there would be 2D engine commands
	w << Macro(WriteHardwareReg, 0x00418800, 0x00000001, 0x00000001);
	w << Macro(WriteHardwareReg, 0x00419A08, 0x00000000, 0x00000010);
	w << Macro(WriteHardwareReg, 0x00419F78, 0x00000000, 0x00000008);
	w << Macro(WriteHardwareReg, 0x00404468, 0x07FFFFFF, 0x3FFFFFFF);
	w << Macro(WriteHardwareReg, 0x00419A04, 0x00000001, 0x00000001);
	w << Macro(WriteHardwareReg, 0x00419A04, 0x00000002, 0x00000002);
	w << CmdInline(3D, ZcullUnknown65a{}, 0x11);
	w << CmdInline(3D, ZcullTestMask{}, 0x00);
	w << CmdInline(3D, ZcullRegion{}, hasZcull() ? 0 : 0x3f);
	w << Cmd(3D, SetInstrumentationMethodHeader{}, 0x49000000);
	w << Cmd(3D, SetInstrumentationMethodData{}, 0x49000001);
	w << Cmd(3D, MmeDriverConstbufIova{}, m_workBuf.getGraphicsCbuf() >> 8, m_workBuf.getGraphicsCbufSize());
	w << Cmd(3D, VertexRunoutBufferIova{}, Iova(m_workBuf.getVtxRunoutBuf()));
	w << Macro(SelectDriverConstbuf, 0);
	w << MacroSetRegisterInArray<E::Bind>(E::Bind::Constbuf{},
		E::Bind::Constbuf::Valid{} | E::Bind::Constbuf::Index{0}
	);

	w << CmdInline(3D, BlendIndependent{}, 1);
	w << CmdInline(3D, EdgeFlag{}, 1);
	w << CmdInline(3D, ViewportTransformEnable{}, 1);
	using VVCC = E::ViewVolumeClipControl;
	w << Cmd(3D, ViewVolumeClipControl{},
		VVCC::ForceDepthRangeZeroToOne{} | VVCC::Unknown1{2} |
		VVCC::DepthClampNear{} | VVCC::DepthClampFar{} |
		VVCC::Unknown11{} | VVCC::Unknown12{1}
	);

	// For MinusOneToOne mode, we need to do this transform: newZ = 0.5*oldZ + 0.5
	// For ZeroToOne mode, the incoming depth value is already in the correct range.
	w << CmdInline(3D, SetDepthMode{}, isDepthModeOpenGL() ? E::SetDepthMode::MinusOneToOne : E::SetDepthMode::ZeroToOne); // this controls pre-transform clipping
	w << MacroSetRegisterInArray<E::ViewportTransform>(E::ViewportTransform::ScaleZ{},
		isDepthModeOpenGL() ? 0.5f : 1.0f
	);
	w << MacroSetRegisterInArray<E::ViewportTransform>(E::ViewportTransform::TranslateZ{},
		isDepthModeOpenGL() ? 0.5f : 0.0f
	);

	// Configure viewport transform XY to convert [-1,1] into [0,1]: newXY = 0.5*oldXY + 0.5
	// Additionally, for UpperLeft origin mode, the Y value needs to be reversed since viewport seems to expect LowerLeft as origin instead.
	// Also, I have no idea what SetWindowOriginMode is affecting, possibly gl_FragCoord?
	w << CmdInline(3D, SetWindowOriginMode{}, isOriginModeOpenGL() ? E::SetWindowOriginMode::LowerLeft : E::SetWindowOriginMode::UpperLeft);
	w << MacroSetRegisterInArray<E::ViewportTransform>(E::ViewportTransform::ScaleX{}, +0.5f);
	w << MacroSetRegisterInArray<E::ViewportTransform>(E::ViewportTransform::ScaleY{}, isOriginModeOpenGL() ? +0.5f : -0.5f);
	w << MacroSetRegisterInArray<E::ViewportTransform>(E::ViewportTransform::TranslateX{}, +0.5f);
	w << MacroSetRegisterInArray<E::ViewportTransform>(E::ViewportTransform::TranslateY{}, +0.5f);
	w << MacroSetRegisterInArray<E::Viewport>(E::Viewport::Horizontal{}, 0U | (1U << 16)); // x=0 w=1
	w << MacroSetRegisterInArray<E::Viewport>(E::Viewport::Vertical{},   0U | (1U << 16)); // y=0 h=1

	w << SetShadowRamControl(SRC::MethodTrack);
	w << MacroSetRegisterInArray<E::Scissor>(E::Scissor::Horizontal{}, 0xFFFF0000);
	w << MacroSetRegisterInArray<E::Scissor>(E::Scissor::Vertical{},   0xFFFF0000);
	w << SetShadowRamControl(SRC::MethodTrackWithFilter);

	w << MacroFillArray<E::MmeProgramIds>(0);
	w << MacroFillArray<E::MmeProgramOffsets>(0);
	w << Cmd(3D, MmeDepthRenderTargetIova{}, 0xFFFFFFFF);

	w << Cmd(3D, IndexArrayLimitIova{}, Iova(0xFFFFFFFFFFUL));
	w << CmdInline(3D, SampleCounterEnable{}, 1);
	w << CmdInline(3D, ClipDistanceEnable{}, 0xFF); // Enable all clip distances
	w << MacroFillArray<E::MsaaMask>(0xFFFF);
	w << CmdInline(3D, ColorReductionEnable{}, 0);
	w << CmdInline(3D, PointSpriteEnable{}, 1);
	w << CmdInline(3D, PointCoordReplace{}, E::PointCoordReplace::Enable{1});
	w << CmdInline(3D, VertexProgramPointSize{}, E::VertexProgramPointSize::Enable{});
	w << CmdInline(3D, PipeNop{}, 0);

	w << CmdInline(3D, StencilFrontFuncMask{}, 0xFF);
	w << CmdInline(3D, StencilFrontMask{}, 0xFF);
	w << CmdInline(3D, StencilBackFuncMask{}, 0xFF);
	w << CmdInline(3D, StencilBackMask{}, 0xFF);

	w << CmdInline(3D, DepthTargetArrayMode{}, E::DepthTargetArrayMode::NumLayers{1});
	w << CmdInline(3D, SetConservativeRasterEnable{}, 0);
	w << Macro(WriteHardwareReg, 0x00418800, 0x00000000, 0x01800000);
	w << CmdInline(3D, Unknown0bb{}, 0);

	w << CmdInline(3D, SetMultisampleRasterEnable{}, 0);
	w << CmdInline(3D, SetCoverageModulationTableEnable{}, 0);
	w << CmdInline(3D, Unknown44c{}, 0x13);
	w << CmdInline(3D, SetMultisampleCoverageToColorEnable{}, 0);

	w << Cmd(3D, SetProgramRegion{}, Iova(getDevice()->getCodeSeg().getBase()));
	w << CmdInline(3D, Unknown5ad{}, 0);

	// The following pgraph register writes apparently initialize the tile cache hardware.
	w << Macro(WriteHardwareReg, 0x00418E40, 0x00000007, 0x0000000F);
	w << Macro(WriteHardwareReg, 0x00418E58, 0x00000842, 0x0000FFFF);
	w << Macro(WriteHardwareReg, 0x00418E40, 0x00000070, 0x000000F0);
	w << Macro(WriteHardwareReg, 0x00418E58, 0x04F10000, 0xFFFF0000);
	w << Macro(WriteHardwareReg, 0x00418E40, 0x00000700, 0x00000F00);
	w << Macro(WriteHardwareReg, 0x00418E5C, 0x00000053, 0x0000FFFF);
	w << Macro(WriteHardwareReg, 0x00418E40, 0x00007000, 0x0000F000);
	w << Macro(WriteHardwareReg, 0x00418E5C, 0x00E90000, 0xFFFF0000);
	w << Macro(WriteHardwareReg, 0x00418E40, 0x00070000, 0x000F0000);
	w << Macro(WriteHardwareReg, 0x00418E60, 0x000000EA, 0x0000FFFF);
	w << Macro(WriteHardwareReg, 0x00418E40, 0x00700000, 0x00F00000);
	w << Macro(WriteHardwareReg, 0x00418E60, 0x00EB0000, 0xFFFF0000);
	w << Macro(WriteHardwareReg, 0x00418E40, 0x07000000, 0x0F000000);
	w << Macro(WriteHardwareReg, 0x00418E64, 0x00000208, 0x0000FFFF);
	w << Macro(WriteHardwareReg, 0x00418E40, 0x70000000, 0xF0000000);
	w << Macro(WriteHardwareReg, 0x00418E64, 0x02090000, 0xFFFF0000);
	w << Macro(WriteHardwareReg, 0x00418E44, 0x00000007, 0x0000000F);
	w << Macro(WriteHardwareReg, 0x00418E68, 0x0000020A, 0x0000FFFF);
	w << Macro(WriteHardwareReg, 0x00418E44, 0x00000070, 0x000000F0);
	w << Macro(WriteHardwareReg, 0x00418E68, 0x020B0000, 0xFFFF0000);
	w << Macro(WriteHardwareReg, 0x00418E44, 0x00000700, 0x00000F00);
	w << Macro(WriteHardwareReg, 0x00418E6C, 0x00000644, 0x0000FFFF);

	w << Cmd(3D, TiledCacheTileSize{}, 0x80 | (0x80 << 16));
	w << Cmd(3D, TiledCacheUnknownConfig0{}, 0x00001109);
	w << Cmd(3D, TiledCacheUnknownConfig1{}, 0x08080202);
	w << Cmd(3D, TiledCacheUnknownConfig2{}, 0x0000001F);
	w << Cmd(3D, TiledCacheUnknownConfig3{}, 0x00080001);
	w << CmdInline(3D, TiledCacheUnkFeatureEnable{}, 0);
}
