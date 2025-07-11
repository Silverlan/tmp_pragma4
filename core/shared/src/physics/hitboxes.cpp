// SPDX-FileCopyrightText: (c) 2019 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#include "stdafx_shared.h"
#include "pragma/model/model.h"
#include "pragma/model/modelmesh.h"
#include "pragma/model/animation/skeleton.hpp"
void Model::AddHitbox(uint32_t boneId, HitGroup group, const Vector3 &min, const Vector3 &max) { AddHitbox(boneId, Hitbox(group, min, max)); }
void Model::AddHitbox(uint32_t boneId, const Hitbox &hitbox)
{
	auto it = m_hitboxes.find(boneId);
	if(it != m_hitboxes.end())
		it->second = hitbox;
	else
		m_hitboxes.insert(decltype(m_hitboxes)::value_type(boneId, hitbox));
}
uint32_t Model::GetHitboxCount() const { return static_cast<uint32_t>(m_hitboxes.size()); }
const std::unordered_map<uint32_t, Hitbox> &Model::GetHitboxes() const { return const_cast<Model *>(this)->GetHitboxes(); }
std::unordered_map<uint32_t, Hitbox> &Model::GetHitboxes() { return m_hitboxes; }
const Hitbox *Model::GetHitbox(uint32_t boneId) const
{
	auto it = m_hitboxes.find(boneId);
	if(it == m_hitboxes.end())
		return nullptr;
	return &it->second;
}
HitGroup Model::GetHitboxGroup(uint32_t boneId) const
{
	auto *hitbox = GetHitbox(boneId);
	if(hitbox == nullptr)
		return HitGroup::Invalid;
	return hitbox->group;
}
bool Model::GetHitboxBounds(uint32_t boneId, Vector3 &min, Vector3 &max) const
{
	auto *hitbox = GetHitbox(boneId);
	if(hitbox == nullptr) {
		min = Vector3 {0.f, 0.f, 0.f};
		max = Vector3 {0.f, 0.f, 0.f};
		return false;
	}
	min = hitbox->min;
	max = hitbox->max;
	return true;
}
std::vector<uint32_t> Model::GetHitboxBones(HitGroup group) const
{
	std::vector<uint32_t> bones;
	GetHitboxBones(group, bones);
	return bones;
}
void Model::GetHitboxBones(HitGroup group, std::vector<uint32_t> &boneIds) const
{
	boneIds.reserve(boneIds.size() + m_hitboxes.size());
	for(auto &it : m_hitboxes) {
		if(it.second.group == group)
			boneIds.push_back(it.first);
	}
}
std::vector<uint32_t> Model::GetHitboxBones() const
{
	std::vector<uint32_t> bones;
	GetHitboxBones(bones);
	return bones;
}
void Model::GetHitboxBones(std::vector<uint32_t> &boneIds) const
{
	boneIds.reserve(boneIds.size() + m_hitboxes.size());
	for(auto &it : m_hitboxes)
		boneIds.push_back(it.first);
}
std::unordered_map<pragma::animation::BoneId, Hitbox> Model::CalcHitboxes() const
{
	struct BoneBounds {
		Vector3 min {std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
		Vector3 max {std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()};
	};
	std::vector<BoneBounds> boneBounds;
	auto &skeleton = GetSkeleton();
	auto numBones = skeleton.GetBoneCount();
	boneBounds.resize(numBones);

	auto &ref = GetReference();
	std::vector<umath::ScaledTransform> bonePoses;
	bonePoses.resize(numBones);
	for(auto i = decltype(numBones) {0u}; i < numBones; ++i) {
		ref.GetBonePose(i, bonePoses[i]);
		bonePoses[i] = bonePoses[i].GetInverse();
	}

	for(auto &meshGroup : m_meshGroups) {
		for(auto &mesh : meshGroup->GetMeshes()) {
			for(auto &subMesh : mesh->GetSubMeshes()) {
				auto &verts = subMesh->GetVertices();
				auto &vertWeights = subMesh->GetVertexWeights();
				for(auto vertIdx = decltype(vertWeights.size()) {0u}; vertIdx < vertWeights.size(); ++vertIdx) {
					auto &v = verts[vertIdx];
					auto &vw = vertWeights[vertIdx];
					constexpr auto n = decltype(vw.boneIds)::length();
					for(auto j = decltype(n) {0}; j < n; ++j) {
						auto boneId = vw.boneIds[j];
						if(boneId < 0)
							continue;
						auto pos = v.position;
						pos = bonePoses[boneId] * pos;
						uvec::min(&boneBounds[boneId].min, pos);
						uvec::max(&boneBounds[boneId].max, pos);
					}
				}
			}
		}
	}

	auto hitboxesAdded = false;
	std::unordered_map<pragma::animation::BoneId, Hitbox> hitboxes;
	hitboxes.reserve(boneBounds.size());
	for(auto boneId = decltype(boneBounds.size()) {0u}; boneId < boneBounds.size(); ++boneId) {
		auto &bounds = boneBounds[boneId];
		if(bounds.min.x == std::numeric_limits<float>::lowest())
			continue;
		if((bounds.max.x - bounds.min.x) > 1.f && (bounds.max.y - bounds.min.y) > 1.f && (bounds.max.z - bounds.min.z) > 1.f) {
			Hitbox hb {HitGroup::Generic, bounds.min, bounds.max};
			hitboxes[boneId] = hb;
		}
	}
	return hitboxes;
}
bool Model::GenerateHitboxes()
{
	if(umath::is_flag_set(m_metaInfo.flags, Flags::GeneratedHitboxes))
		return false;
	umath::set_flag(m_metaInfo.flags, Flags::GeneratedHitboxes);
	auto hitboxes = CalcHitboxes();
	for(auto &[boneId, hb] : hitboxes)
		AddHitbox(boneId, hb);
	return !hitboxes.empty();
}
