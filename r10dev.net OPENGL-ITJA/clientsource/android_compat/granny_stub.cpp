#ifdef __ANDROID__
#include <granny.h>

extern "C" {
GRANNY_DYNLINK(void) GrannyCompleteControlAt(granny_control* Control, granny_real32 AtSeconds) {}
GRANNY_DYNLINK(bool) GrannyControlIsComplete(granny_control const* Control) { return false; }
GRANNY_DYNLINK(void) GrannyConvertSingleObject(granny_data_type_definition const* SourceType, void const* SourceObject, granny_data_type_definition const* DestType, void* DestObject, granny_conversion_handler* OverrideHandler) {}
GRANNY_DYNLINK(void) GrannyCopyMeshIndices(granny_mesh const* Mesh, granny_int32x BytesPerIndex, void* DestIndices) {}
GRANNY_DYNLINK(void) GrannyCopyMeshVertices(granny_mesh const* Mesh, granny_data_type_definition const* VertexType, void* DestVertices) {}
GRANNY_DYNLINK(void) GrannyDeformVertices(granny_mesh_deformer const* Deformer, granny_int32x const* MatrixIndices, granny_real32 const* MatrixBuffer4x4, granny_int32x VertexCount, void const* SourceVertices, void* DestVertices) {}
GRANNY_DYNLINK(bool) GrannyFindBoneByName(granny_skeleton const* Skeleton, char const* BoneName, granny_int32x* BoneIndex) { return false; }
GRANNY_DYNLINK(bool) GrannyFindMatchingMember(granny_data_type_definition const* SourceType, void const* SourceObject, char const* DestMemberName, granny_variant* Result) { return false; }
GRANNY_DYNLINK(void) GrannyFreeCompletedModelControls(granny_model_instance const* ModelInstance) {}
GRANNY_DYNLINK(void) GrannyFreeControl(granny_control* Control) {}
GRANNY_DYNLINK(bool) GrannyFreeControlIfComplete(granny_control* Control) { return false; }
GRANNY_DYNLINK(void) GrannyFreeControlOnceUnused(granny_control* Control) {}
GRANNY_DYNLINK(void) GrannyFreeFile(granny_file* File) {}
GRANNY_DYNLINK(void) GrannyFreeFileSection(granny_file* File, granny_int32x SectionIndex) {}
GRANNY_DYNLINK(void) GrannyFreeLocalPose(granny_local_pose* LocalPose) {}
GRANNY_DYNLINK(void) GrannyFreeMeshBinding(granny_mesh_binding* Binding) {}
GRANNY_DYNLINK(void) GrannyFreeMeshDeformer(granny_mesh_deformer* Deformer) {}
GRANNY_DYNLINK(void) GrannyFreeModelInstance(granny_model_instance* ModelInstance) {}
GRANNY_DYNLINK(void) GrannyFreeWorldPose(granny_world_pose* WorldPose) {}
GRANNY_DYNLINK(granny_real32) GrannyGetControlLocalDuration(granny_control const* Control) { return 0; }
GRANNY_DYNLINK(granny_int32x) GrannyGetControlLoopCount(granny_control const* Control) { return 0; }
GRANNY_DYNLINK(granny_real32) GrannyGetControlRawLocalClock(granny_control* Control) { return 0; }
GRANNY_DYNLINK(granny_real32) GrannyGetControlSpeed(granny_control const* Control) { return 0; }
GRANNY_DYNLINK(granny_file_info*) GrannyGetFileInfo(granny_file* File) { return 0; }
GRANNY_DYNLINK(char const*) GrannyGetLogMessageOriginString(granny_log_message_origin Origin) { return ""; }
GRANNY_DYNLINK(char const*) GrannyGetLogMessageTypeString(granny_log_message_type Type) { return ""; }
GRANNY_DYNLINK(granny_texture*) GrannyGetMaterialTextureByType(granny_material const* Material, granny_material_texture_type Type) { return 0; }
GRANNY_DYNLINK(granny_int32x const*) GrannyGetMeshBindingToBoneIndices(granny_mesh_binding const* Binding) { return 0; }
GRANNY_DYNLINK(granny_int32x) GrannyGetMeshIndexCount(granny_mesh const* Mesh) { return 0; }
GRANNY_DYNLINK(granny_int32x) GrannyGetMeshTriangleGroupCount(granny_mesh const* Mesh) { return 0; }
GRANNY_DYNLINK(granny_tri_material_group*) GrannyGetMeshTriangleGroups(granny_mesh const* Mesh) { return 0; }
GRANNY_DYNLINK(granny_int32x) GrannyGetMeshVertexCount(granny_mesh const* Mesh) { return 0; }
GRANNY_DYNLINK(granny_data_type_definition*) GrannyGetMeshVertexType(granny_mesh const* Mesh) { return 0; }
GRANNY_DYNLINK(void*) GrannyGetMeshVertices(granny_mesh const* Mesh) { return 0; }
GRANNY_DYNLINK(granny_skeleton*) GrannyGetSourceSkeleton(granny_model_instance const* Model) { return 0; }
GRANNY_DYNLINK(granny_int32x) GrannyGetTotalTypeSize(granny_data_type_definition const* TypeDefinition) { return 0; }
GRANNY_DYNLINK(granny_real32*) GrannyGetWorldPose4x4(granny_world_pose const* WorldPose, granny_int32x BoneIndex) { return 0; }
GRANNY_DYNLINK(granny_real32*) GrannyGetWorldPoseComposite4x4(granny_world_pose const* WorldPose, granny_int32x BoneIndex) { return 0; }
GRANNY_DYNLINK(granny_matrix_4x4*) GrannyGetWorldPoseComposite4x4Array(granny_world_pose const* WorldPose) { return 0; }
GRANNY_DYNLINK(granny_model_instance*) GrannyInstantiateModel(granny_model const* Model) { return 0; }
GRANNY_DYNLINK(bool) GrannyMeshIsRigid(granny_mesh const* Mesh) { return false; }
GRANNY_DYNLINK(granny_local_pose*) GrannyNewLocalPose(granny_int32x BoneCount) { return 0; }
GRANNY_DYNLINK(granny_mesh_binding*) GrannyNewMeshBinding(granny_mesh const* Mesh, granny_skeleton const* FromSkeleton, granny_skeleton const* ToSkeleton) { return 0; }
GRANNY_DYNLINK(granny_mesh_deformer*) GrannyNewMeshDeformer(granny_data_type_definition const* InputVertexLayout, granny_data_type_definition const* OutputVertexLayout, granny_deformation_type DeformationType, granny_deformer_tail_flags TailFlag) { return 0; }
GRANNY_DYNLINK(granny_world_pose*) GrannyNewWorldPose(granny_int32x BoneCount) { return 0; }
GRANNY_DYNLINK(granny_control*) GrannyPlayControlledAnimation(granny_real32 StartTime, granny_animation const* Animation, granny_model_instance* Model) { return 0; }
GRANNY_DYNLINK(granny_file*) GrannyReadEntireFileFromMemory(granny_int32x MemorySize, void const* Memory) { return 0; }
GRANNY_DYNLINK(void) GrannySampleModelAnimationsAccelerated(granny_model_instance const* ModelInstance, granny_int32x BoneCount, granny_real32 const* Offset4x4, granny_local_pose* Scratch, granny_world_pose* Result) {}
GRANNY_DYNLINK(void) GrannySetControlEaseIn(granny_control* Control, bool EaseIn) {}
GRANNY_DYNLINK(void) GrannySetControlEaseInCurve(granny_control* Control, granny_real32 StartSeconds, granny_real32 EndSeconds, granny_real32 StartValue, granny_real32 StartTangent, granny_real32 EndTangent, granny_real32 EndValue) {}
GRANNY_DYNLINK(void) GrannySetControlEaseOut(granny_control* Control, bool EaseOut) {}
GRANNY_DYNLINK(void) GrannySetControlEaseOutCurve(granny_control* Control, granny_real32 StartSeconds, granny_real32 EndSeconds, granny_real32 StartValue, granny_real32 StartTangent, granny_real32 EndTangent, granny_real32 EndValue) {}
GRANNY_DYNLINK(void) GrannySetControlLoopCount(granny_control* Control, granny_int32x LoopCount) {}
GRANNY_DYNLINK(void) GrannySetControlRawLocalClock(granny_control* Control, granny_real32 LocalClock) {}
GRANNY_DYNLINK(void) GrannySetControlSpeed(granny_control* Control, granny_real32 Speed) {}
GRANNY_DYNLINK(void) GrannySetLogCallback(granny_log_callback const* LogCallback) {}
GRANNY_DYNLINK(void) GrannySetModelClock(granny_model_instance const* ModelInstance, granny_real32 NewClock) {}
GRANNY_DYNLINK(void) GrannyUpdateModelMatrix(granny_model_instance const* ModelInstance, granny_real32 SecondsElapsed, granny_real32 const* ModelMatrix4x4, granny_real32* DestMatrix4x4, bool Inverse) {}
}

static granny_data_type_definition s_PNT332VertexType[] = { { GrannyEndMember } };
GRANNY_DYNLINKDATA(granny_data_type_definition*) GrannyPNT332VertexType = s_PNT332VertexType;
#endif
