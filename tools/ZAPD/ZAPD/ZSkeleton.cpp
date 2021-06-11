#include "ZSkeleton.h"
#include "BitConverter.h"
#include "HighLevel/HLModelIntermediette.h"
#include "StringHelper.h"

REGISTER_ZFILENODE(Skeleton, ZSkeleton);

ZSkeleton::ZSkeleton(ZFile* nParent) : ZResource(nParent)
{
	type = ZSkeletonType::Normal;
	limbType = ZLimbType::Standard;
	dListCount = 0;

	RegisterRequiredAttribute("Type");
	RegisterRequiredAttribute("LimbType");
}

ZSkeleton::ZSkeleton(ZSkeletonType nType, ZLimbType nLimbType, const std::string& prefix,
                     uint32_t nRawDataIndex, ZFile* nParent)
	: ZSkeleton(nParent)
{
	rawDataIndex = nRawDataIndex;
	parent = nParent;

	name = StringHelper::Sprintf("%sSkel_%06X", prefix.c_str(), rawDataIndex);
	type = nType;
	limbType = nLimbType;

	ParseRawData();

	std::string defaultPrefix = name;
	defaultPrefix.replace(0, 1, "s");  // replace g prefix with s for local variables
	uint32_t ptr = Seg2Filespace(limbsArrayAddress, parent->baseAddress);

	for (size_t i = 0; i < limbCount; i++)
	{
		uint32_t ptr2 =
			Seg2Filespace(BitConverter::ToUInt32BE(parent->GetRawData(), ptr), parent->baseAddress);

		ZLimb* limb = new ZLimb(limbType, prefix, ptr2, parent);
		limbs.push_back(limb);

		ptr += 4;
	}
}

ZSkeleton::~ZSkeleton()
{
	for (auto& limb : limbs)
		delete limb;
}

void ZSkeleton::ParseXML(tinyxml2::XMLElement* reader)
{
	ZResource::ParseXML(reader);

	std::string skelTypeXml = registeredAttributes.at("Type").value;

	if (skelTypeXml == "Flex")
		type = ZSkeletonType::Flex;
	else if (skelTypeXml == "Curve")
		type = ZSkeletonType::Curve;
	else if (skelTypeXml != "Normal")
	{
		fprintf(stderr,
		        "ZSkeleton::ParseXML: Warning in '%s'.\n"
		        "\t Invalid Type found: '%s'.\n"
		        "\t Defaulting to 'Normal'.\n",
		        name.c_str(), skelTypeXml.c_str());
		type = ZSkeletonType::Normal;
	}

	std::string limbTypeXml = registeredAttributes.at("LimbType").value;

	if (limbTypeXml == "Standard")
		limbType = ZLimbType::Standard;
	else if (limbTypeXml == "LOD")
		limbType = ZLimbType::LOD;
	else if (limbTypeXml == "Skin")
		limbType = ZLimbType::Skin;
	else if (limbTypeXml == "Curve")
		limbType = ZLimbType::Curve;
	else
	{
		fprintf(stderr,
		        "ZSkeleton::ParseXML: Warning in '%s'.\n"
		        "\t Invalid LimbType found: '%s'.\n"
		        "\t Defaulting to 'Standard'.\n",
		        name.c_str(), limbTypeXml.c_str());
		limbType = ZLimbType::Standard;
	}
}

void ZSkeleton::ParseRawData()
{
	ZResource::ParseRawData();

	const auto& rawData = parent->GetRawData();
	limbsArrayAddress = BitConverter::ToUInt32BE(rawData, rawDataIndex);
	limbCount = BitConverter::ToUInt8BE(rawData, rawDataIndex + 4);
	dListCount = BitConverter::ToUInt8BE(rawData, rawDataIndex + 8);
}

void ZSkeleton::ExtractFromXML(tinyxml2::XMLElement* reader, uint32_t nRawDataIndex)
{
	ZResource::ExtractFromXML(reader, nRawDataIndex);

	parent->AddDeclaration(rawDataIndex, DeclarationAlignment::Align16, GetRawDataSize(),
	                       GetSourceTypeName(), name, "");

	std::string defaultPrefix = name;
	defaultPrefix.replace(0, 1, "s");  // replace g prefix with s for local variables
	uint32_t ptr = Seg2Filespace(limbsArrayAddress, parent->baseAddress);

	const auto& rawData = parent->GetRawData();
	for (size_t i = 0; i < limbCount; i++)
	{
		uint32_t ptr2 = Seg2Filespace(BitConverter::ToUInt32BE(rawData, ptr), parent->baseAddress);

		std::string limbName = StringHelper::Sprintf("%sLimb_%06X", defaultPrefix.c_str(), ptr2);
		Declaration* decl = parent->GetDeclaration(ptr2);
		if (decl != nullptr)
			limbName = decl->varName;

		ZLimb* limb = new ZLimb(parent);
		limb->SetLimbType(limbType);
		limb->SetName(limbName);
		limb->ExtractFromXML(nullptr, ptr2);
		limbs.push_back(limb);

		ptr += 4;
	}
}

void ZSkeleton::GenerateHLIntermediette(HLFileIntermediette& hlFile)
{
	HLModelIntermediette* mdl = (HLModelIntermediette*)&hlFile;
	HLModelIntermediette::FromZSkeleton(mdl, this);
	mdl->blocks.push_back(new HLTerminator());
}

size_t ZSkeleton::GetRawDataSize() const
{
	switch (type)
	{
	case ZSkeletonType::Flex:
		return 0xC;
	case ZSkeletonType::Normal:
	case ZSkeletonType::Curve:
	default:
		return 0x8;
	}
}

std::string ZSkeleton::GetSourceOutputCode(const std::string& prefix)
{
	if (parent == nullptr)
		return "";

	std::string defaultPrefix = name.c_str();
	defaultPrefix.replace(0, 1, "s");  // replace g prefix with s for local variables

	for (auto& limb : limbs)
		limb->GetSourceOutputCode(defaultPrefix);

	uint32_t ptr = Seg2Filespace(limbsArrayAddress, parent->baseAddress);
	if (!parent->HasDeclaration(ptr))
	{
		// Table
		std::string tblStr = "";
		std::string limbArrTypeStr = "static void*";
		if (limbType == ZLimbType::Curve)
		{
			limbArrTypeStr =
				StringHelper::Sprintf("static %s*", ZLimb::GetSourceTypeName(limbType));
		}

		for (size_t i = 0; i < limbs.size(); i++)
		{
			ZLimb* limb = limbs.at(i);

			std::string decl = StringHelper::Sprintf(
				"    &%s,", parent->GetDeclarationName(limb->GetFileAddress()).c_str());
			if (i != (limbs.size() - 1))
			{
				decl += "\n";
			}

			tblStr += decl;
		}

		parent->AddDeclarationArray(ptr, DeclarationAlignment::None, 4 * limbCount, limbArrTypeStr,
		                            StringHelper::Sprintf("%sLimbs", defaultPrefix.c_str()),
		                            limbCount, tblStr);
	}

	std::string headerStr;
	switch (type)
	{
	case ZSkeletonType::Normal:
	case ZSkeletonType::Curve:
		headerStr = StringHelper::Sprintf("\n\t%sLimbs, %i\n", defaultPrefix.c_str(), limbCount);
		break;
	case ZSkeletonType::Flex:
		headerStr = StringHelper::Sprintf("\n\t{ %sLimbs, %i }, %i\n", defaultPrefix.c_str(),
		                                  limbCount, dListCount);
		break;
	}

	Declaration* decl = parent->GetDeclaration(GetAddress());

	if (decl == nullptr)
	{
		parent->AddDeclaration(GetAddress(), DeclarationAlignment::Align16, GetRawDataSize(),
		                       GetSourceTypeName(), name, headerStr);
	}
	else
	{
		decl->text = headerStr;
	}

	return "";
}

std::string ZSkeleton::GetSourceTypeName() const
{
	switch (type)
	{
	case ZSkeletonType::Normal:
		return "SkeletonHeader";
	case ZSkeletonType::Flex:
		return "FlexSkeletonHeader";
	case ZSkeletonType::Curve:
		return "SkelCurveLimbList";
	}

	return "SkeletonHeader";
}

ZResourceType ZSkeleton::GetResourceType() const
{
	return ZResourceType::Skeleton;
}

segptr_t ZSkeleton::GetAddress()
{
	return rawDataIndex;
}

uint8_t ZSkeleton::GetLimbCount()
{
	return limbCount;
}