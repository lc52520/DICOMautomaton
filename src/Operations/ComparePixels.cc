//ComparePixels.cc - A part of DICOMautomaton 2019. Written by hal clark.

#include <experimental/optional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    
#include <utility>            //Needed for std::pair.
#include <vector>

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../YgorImages_Functors/Compute/Compare_Images.h"
#include "ComparePixels.h"
#include "YgorImages.h"
#include "YgorString.h"       //Needed for GetFirstRegex(...)



OperationDoc OpArgDocComparePixels(void){
    OperationDoc out;
    out.name = "ComparePixels";
    out.desc = 
        "This operation compares images ('test' images and 'reference' images) on a per-voxel/per-pixel basis."
        " Any combination of 2D and 3D images is supported, including images which do not fully overlap, but the"
        " reference image array must be rectilinear (this property is verified).";

    out.notes.emplace_back(
        "Images are overwritten, but ReferenceImages are not."
        " Multiple Images may be specified, but only one ReferenceImages may be specified."
    );
    out.notes.emplace_back(
        "The reference image array must be rectilinear. (This is a requirement specific to this"
        " implementation, a less restrictive implementation could overcome the issue.)"
    );
    out.notes.emplace_back(
        "For the fastest and most accurate results, test and reference image arrays should spatially align."
        " However, alignment is **not** necessary. If test and reference image arrays are aligned,"
        " image adjacency can be precomputed and the analysis will be faster. If not, image adjacency"
        " must be evaluated for every voxel."
    );
    out.notes.emplace_back(
        "This operation does **not** make use of interpolation for any comparison."
        " Only direct voxel-to-voxel comparisons are used."
        " Implicit interpolation is used (via the intermediate value theorem) for the"
        " distance-to-agreement comparison."
        " For this reason, the accuracy of all comparisons should be expected to be limited by"
        " image spatial resolution (i.e., voxel dimensions). For example, accuracy of the"
        " distance-to-agreement comparison is limited to the largest voxel dimension (in the"
        " worst case). Reference images should be supersampled if necessary."
    );
    out.notes.emplace_back(
        "The distance-to-agreement comparison will tend to overestimate the distance, especially"
        " when the DTA value is low, because only implicit interpolation is used."
        " Reference images should be supersampled if necessary."
    );


    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "all";

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ReferenceImageSelection";
    out.args.back().default_val = "all";

    out.args.emplace_back();
    out.args.back().name = "NormalizedROILabelRegex";
    out.args.back().desc = "A regex matching ROI labels/names to consider. The default will match"
                      " all available ROIs. Be aware that input spaces are trimmed to a single space."
                      " If your ROI name has more than two sequential spaces, use regex to avoid them."
                      " All ROIs have to match the single regex, so use the 'or' token if needed."
                      " Regex is case insensitive and uses extended POSIX syntax.";
    out.args.back().default_val = ".*";
    out.args.back().expected = true;
    out.args.back().examples = { ".*", ".*Body.*", "Body", "Gross_Liver",
                            R"***(.*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*)***",
                            R"***(Left Parotid|Right Parotid)***" };

    out.args.emplace_back();
    out.args.back().name = "ROILabelRegex";
    out.args.back().desc = "A regex matching ROI labels/names to consider. The default will match"
                      " all available ROIs. Be aware that input spaces are trimmed to a single space."
                      " If your ROI name has more than two sequential spaces, use regex to avoid them."
                      " All ROIs have to match the single regex, so use the 'or' token if needed."
                      " Regex is case insensitive and uses extended POSIX syntax.";
    out.args.back().default_val = ".*";
    out.args.back().expected = true;
    out.args.back().examples = { ".*", ".*body.*", "body", "Gross_Liver",
                            R"***(.*left.*parotid.*|.*right.*parotid.*|.*eyes.*)***",
                            R"***(left_parotid|right_parotid)***" };

    out.args.emplace_back();
    out.args.back().name = "Method";
    out.args.back().desc = "The comparison method to compute. Three options are currently available:"
                           " distance-to-agreement (DTA), discrepancy, and gamma-index."
                           " All three are fully 3D, but can also work for 2D or mixed 2D-3D comparisons."
                           " DTA is a measure of how far away the nearest voxel (in the reference images)"
                           " is with a voxel intensity sufficiently close to each voxel in the test images."
                           " This comparison ignores pixel intensities except to test if the values match"
                           " within the specified tolerance. The voxel neighbourhood is exhaustively"
                           " explored until a suitable voxel is found. Implicit interpolation is used to"
                           " detect when the value could be found via interpolation, but explicit"
                           " interpolation is not used. Thus distance might be overestimated."
                           " A discrepancy comparison measures the point-dose intensity discrepancy without"
                           " accounting for spatial shifts."
                           " A gamma analysis combines distance-to-agreement and point dose differences into"
                           " a single index which is best used to test if both DTA and discrepancy criteria"
                           " are satisfied (gamma <= 1 iff both pass). It was proposed by Low et al. in 1998"
                           " ((doi:10.1118/1.598248). Gamma analyses permits trade-offs between spatial"
                           " and dosimetric discrepancies which can arise when the image arrays slightly differ"
                           " in alignment or pixel values.";
    out.args.back().default_val = "gamma-index";
    out.args.back().expected = true;
    out.args.back().examples = { "gamma-index",
                                 "DTA",
                                 "discrepancy" };

    out.args.emplace_back();
    out.args.back().name = "Channel";
    out.args.back().desc = "The channel to compare (zero-based)."
                           " Note that both test images and reference images will share this specifier.";
    out.args.back().default_val = "0";
    out.args.back().expected = true;
    out.args.back().examples = { "0",
                                 "1",
                                 "2" };

    out.args.emplace_back();
    out.args.back().name = "TestImgLowerThreshold";
    out.args.back().desc = "Pixel lower threshold for the test images."
                           " Only voxels with values above this threshold (inclusive) will be altered.";
    out.args.back().default_val = "-inf";
    out.args.back().expected = true;
    out.args.back().examples = { "-inf",
                                 "0.0",
                                 "200" };

    out.args.emplace_back();
    out.args.back().name = "TestImgUpperThreshold";
    out.args.back().desc = "Pixel upper threshold for the test images."
                           " Only voxels with values below this threshold (inclusive) will be altered.";
    out.args.back().default_val = "inf";
    out.args.back().expected = true;
    out.args.back().examples = { "inf",
                                 "1.23",
                                 "1000" };

    out.args.emplace_back();
    out.args.back().name = "RefImgLowerThreshold";
    out.args.back().desc = "Pixel lower threshold for the reference images."
                           " Only voxels with values above this threshold (inclusive) will be altered.";
    out.args.back().default_val = "-inf";
    out.args.back().expected = true;
    out.args.back().examples = { "-inf",
                                 "0.0",
                                 "200" };

    out.args.emplace_back();
    out.args.back().name = "RefImgUpperThreshold";
    out.args.back().desc = "Pixel upper threshold for the reference images."
                           " Only voxels with values below this threshold (inclusive) will be altered.";
    out.args.back().default_val = "inf";
    out.args.back().expected = true;
    out.args.back().examples = { "inf",
                                 "1.23",
                                 "1000" };

    out.args.emplace_back();
    out.args.back().name = "DTAVoxValEqAbs";
    out.args.back().desc = "Parameter for all comparisons involving a distance-to-agreement (DTA) search."
                           " The difference in voxel values considered to be sufficiently equal (absolute;"
                           " in voxel intensity units). Note: This value CAN be zero. It is meant to"
                           " help overcome noise.";
    out.args.back().default_val = "1.0E-3";
    out.args.back().expected = true;
    out.args.back().examples = { "1.0E-3",
                                 "1.0E-5",
                                 "0.0",
                                 "0.5" };

    out.args.emplace_back();
    out.args.back().name = "DTAVoxValEqRelDiff";
    out.args.back().desc = "Parameter for all comparisons involving a distance-to-agreement (DTA) search."
                           " The difference in voxel values considered to be sufficiently equal (~relative"
                           " difference; in %). Note: This value CAN be zero. It is meant to help overcome"
                           " noise.";
    out.args.back().default_val = "1.0";
    out.args.back().expected = true;
    out.args.back().examples = { "0.1",
                                 "1.0",
                                 "10.0" };

    out.args.emplace_back();
    out.args.back().name = "DTAMax";
    out.args.back().desc = "Parameter for all comparisons involving a distance-to-agreement (DTA) search."
                           " Maximally acceptable distance-to-agreement (in DICOM units: mm) above which to"
                           " stop searching. All voxels within this distance will be searched unless a"
                           " matching voxel is found. Note that a gamma-index comparison may terminate"
                           " this search early if the gamma-index is known to be greater than one."
                           " It is recommended to make this value approximately 1 voxel width larger than"
                           " necessary in case a matching voxel can be located near the boundary."
                           " Also note that some voxels beyond the DTA_max distance may be evaluated.";
    out.args.back().default_val = "30.0";
    out.args.back().expected = true;
    out.args.back().examples = { "3.0",
                                 "5.0",
                                 "50.0" };

    out.args.emplace_back();
    out.args.back().name = "GammaDTAThreshold";
    out.args.back().desc = "Parameter for gamma-index comparisons."
                           " Maximally acceptable distance-to-agreement (in DICOM units: mm). When the measured DTA"
                           " is above this value, the gamma index will necessarily be greater than one."
                           " Note this parameter can differ from the DTA_max search cut-off, but should be <= to it.";
    out.args.back().default_val = "5.0";
    out.args.back().expected = true;
    out.args.back().examples = { "3.0",
                                 "5.0",
                                 "10.0" };

    out.args.emplace_back();
    out.args.back().name = "GammaDiscThreshold";
    out.args.back().desc = "Parameter for gamma-index comparisons."
                           " Voxel value relative discrepancy (relative difference; in %)."
                           " When the measured discrepancy is above this value, the gamma index will necessarily"
                           " be greater than one.";
    out.args.back().default_val = "5.0";
    out.args.back().expected = true;
    out.args.back().examples = { "3.0",
                                 "5.0",
                                 "10.0" };

    out.args.emplace_back();
    out.args.back().name = "GammaTerminateAboveOne";
    out.args.back().desc = "Parameter for gamma-index comparisons."
                           " Halt spatial searching if the gamma index will necessarily indicate failure (i.e.,"
                           " gamma >1). Note this can parameter can drastically reduce the computational effort"
                           " required to compute the gamma index, but the reported gamma values will be invalid"
                           " whenever they are >1. This is often tolerable since the magnitude only matters when"
                           " it is <1. In lieu of the true gamma-index, a value slightly >1 will be assumed.";
    out.args.back().default_val = "true";
    out.args.back().expected = true;
    out.args.back().examples = { "true",
                                 "false" };

    return out;
}



Drover ComparePixels(Drover DICOM_data, 
                           OperationArgPkg OptArgs, 
                           std::map<std::string,std::string> /*InvocationMetadata*/, 
                           std::string /*FilenameLex*/ ){


    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();
    const auto ReferenceImageSelectionStr = OptArgs.getValueStr("ReferenceImageSelection").value();

    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();

    const auto MethodStr = OptArgs.getValueStr("Method").value();
    const auto Channel = std::stol( OptArgs.getValueStr("Channel").value() );
    const auto TestImgLowerThreshold = std::stod( OptArgs.getValueStr("TestImgLowerThreshold").value() );
    const auto TestImgUpperThreshold = std::stod( OptArgs.getValueStr("TestImgUpperThreshold").value() );
    const auto RefImgLowerThreshold = std::stod( OptArgs.getValueStr("RefImgLowerThreshold").value() );
    const auto RefImgUpperThreshold = std::stod( OptArgs.getValueStr("RefImgUpperThreshold").value() );
    const auto DTAVoxValEqAbs = std::stod( OptArgs.getValueStr("DTAVoxValEqAbs").value() );
    const auto DTAVoxValEqRelDiff = std::stod( OptArgs.getValueStr("DTAVoxValEqRelDiff").value() );
    const auto DTAMax = std::stod( OptArgs.getValueStr("DTAMax").value() );
    const auto GammaDTAThreshold = std::stod( OptArgs.getValueStr("GammaDTAThreshold").value() );
    const auto GammaDiscThreshold = std::stod( OptArgs.getValueStr("GammaDiscThreshold").value() );
    const auto GammaTerminateAboveOneStr = OptArgs.getValueStr("GammaTerminateAboveOne").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto method_gam = Compile_Regex("^ga?m?m?a?-?i?n?d?e?x?$");
    const auto method_dta = Compile_Regex("^dta?$");
    const auto method_dis = Compile_Regex("^dis?c?r?e?p?a?n?c?y?$");
    const auto regex_true = Compile_Regex("^tr?u?e?$");

    const auto GammaTerminateAboveOne = std::regex_match(GammaTerminateAboveOneStr, regex_true);
    //-----------------------------------------------------------------------------------------------------------------

    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    auto cc_all = All_CCs( DICOM_data );
    auto cc_ROIs = Whitelist( cc_all, { { "ROIName", ROILabelRegex },
                                        { "NormalizedROIName", NormalizedROILabelRegex } } );
    if(cc_ROIs.empty()){
        throw std::invalid_argument("No contours selected. Cannot continue.");
    }

    auto RIAs_all = All_IAs( DICOM_data );
    auto RIAs = Whitelist( RIAs_all, ReferenceImageSelectionStr );
    if(RIAs.size() != 1){
        throw std::invalid_argument("Only one reference image collection can be specified.");
    }
    std::list<std::reference_wrapper<planar_image_collection<float, double>>> RIARL = { std::ref( (*( RIAs.front() ))->imagecoll ) };


    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    for(auto & iap_it : IAs){

        ComputeCompareImagesUserData ud;

        if(false){
        }else if(std::regex_match(MethodStr, method_gam)){
            ud.comparison_method = ComputeCompareImagesUserData::ComparisonMethod::GammaIndex;
        }else if(std::regex_match(MethodStr, method_dta)){
            ud.comparison_method = ComputeCompareImagesUserData::ComparisonMethod::DTA;
        }else if(std::regex_match(MethodStr, method_dis)){
            ud.comparison_method = ComputeCompareImagesUserData::ComparisonMethod::Discrepancy;
        }else{
            throw std::invalid_argument("Method not understood. Cannot continue.");
        }

        ud.channel = Channel;

        ud.inc_lower_threshold = TestImgLowerThreshold;
        ud.inc_upper_threshold = TestImgUpperThreshold;
        ud.ref_img_inc_lower_threshold = RefImgLowerThreshold;
        ud.ref_img_inc_upper_threshold = RefImgUpperThreshold;


        ud.DTA_vox_val_eq_abs = DTAVoxValEqAbs;
        ud.DTA_vox_val_eq_reldiff = DTAVoxValEqRelDiff;
        ud.DTA_max = DTAMax;

        ud.gamma_DTA_threshold = GammaDTAThreshold;
        ud.gamma_Dis_reldiff_threshold = GammaDiscThreshold;

        ud.gamma_terminate_when_max_exceeded = GammaTerminateAboveOne;
        //ud.gamma_terminated_early = std::nextafter(1.0, std::numeric_limits<double>::infinity());

        if(!(*iap_it)->imagecoll.Compute_Images( ComputeCompareImages, 
                                                 RIARL, cc_ROIs, &ud )){
            throw std::runtime_error("Unable to compare images.");
        }
    }

    return DICOM_data;
}