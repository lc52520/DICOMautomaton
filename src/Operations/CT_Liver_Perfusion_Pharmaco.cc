//CT_Liver_Perfusion_Pharmaco.cc - A part of DICOMautomaton 2015, 2016. Written by hal clark.

#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <string>    
#include <vector>
#include <set> 
#include <map>
#include <unordered_map>
#include <list>
#include <functional>
#include <thread>
#include <array>
#include <mutex>
#include <limits>
#include <cmath>

#include <unistd.h>           //fork().
#include <stdlib.h>           //quick_exit(), EXIT_SUCCESS.
#include <getopt.h>           //Needed for 'getopts' argument parsing.
#include <cstdlib>            //Needed for exit() calls.
#include <utility>            //Needed for std::pair.
#include <algorithm>
#include <experimental/optional>

#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>

#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMathPlottingGnuplot.h" //Needed for YgorMathPlottingGnuplot::*.
#include "YgorMathChebyshev.h" //Needed for cheby_approx class.
#include "YgorMathBSpline.h"   //Needed for basis_spline class.
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorContainers.h"   //Needed for bimap class.
#include "YgorPerformance.h"  //Needed for YgorPerformance_dt_from_last().
#include "YgorAlgorithms.h"   //Needed for For_Each_In_Parallel<..>(...)
#include "YgorArguments.h"    //Needed for ArgumentHandler class.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorImages.h"
#include "YgorImagesIO.h"
#include "YgorImagesPlotting.h"

#include "Explicator.h"       //Needed for Explicator class.

#include "../Structs.h"

#include "../Common_Plotting.h"

#include "../YgorImages_Functors/Grouping/Misc_Functors.h"

#include "../YgorImages_Functors/Processing/DCEMRI_AUC_Map.h"
#include "../YgorImages_Functors/Processing/DCEMRI_S0_Map.h"
#include "../YgorImages_Functors/Processing/DCEMRI_T1_Map.h"
#include "../YgorImages_Functors/Processing/Highlight_ROI_Voxels.h"
#include "../YgorImages_Functors/Processing/Kitchen_Sink_Analysis.h"
#include "../YgorImages_Functors/Processing/IVIMMRI_ADC_Map.h"
#include "../YgorImages_Functors/Processing/Time_Course_Slope_Map.h"
#include "../YgorImages_Functors/Processing/CT_Perfusion_Clip_Search.h"
#include "../YgorImages_Functors/Processing/CT_Perf_Pixel_Filter.h"
#include "../YgorImages_Functors/Processing/CT_Convert_NaNs_to_Air.h"
#include "../YgorImages_Functors/Processing/Min_Pixel_Value.h"
#include "../YgorImages_Functors/Processing/Max_Pixel_Value.h"
#include "../YgorImages_Functors/Processing/CT_Reasonable_HU_Window.h"
#include "../YgorImages_Functors/Processing/Slope_Difference.h"
#include "../YgorImages_Functors/Processing/Centralized_Moments.h"
#include "../YgorImages_Functors/Processing/Logarithmic_Pixel_Scale.h"
#include "../YgorImages_Functors/Processing/Per_ROI_Time_Courses.h"
#include "../YgorImages_Functors/Processing/DBSCAN_Time_Courses.h"
#include "../YgorImages_Functors/Processing/In_Image_Plane_Bilinear_Supersample.h"
#include "../YgorImages_Functors/Processing/In_Image_Plane_Bicubic_Supersample.h"
#include "../YgorImages_Functors/Processing/In_Image_Plane_Pixel_Decimate.h"
#include "../YgorImages_Functors/Processing/Cross_Second_Derivative.h"
#include "../YgorImages_Functors/Processing/Liver_Pharmacokinetic_Model_5Param_Linear.h"
#include "../YgorImages_Functors/Processing/Liver_Pharmacokinetic_Model_5Param_Cheby.h"
#include "../YgorImages_Functors/Processing/Orthogonal_Slices.h"

#include "../YgorImages_Functors/Transform/DCEMRI_C_Map.h"
#include "../YgorImages_Functors/Transform/DCEMRI_Signal_Difference_C.h"
#include "../YgorImages_Functors/Transform/CT_Perfusion_Signal_Diff.h"
#include "../YgorImages_Functors/Transform/DCEMRI_S0_Map_v2.h"
#include "../YgorImages_Functors/Transform/DCEMRI_T1_Map_v2.h"
#include "../YgorImages_Functors/Transform/Pixel_Value_Histogram.h"
#include "../YgorImages_Functors/Transform/Subtract_Spatially_Overlapping_Images.h"

#include "../YgorImages_Functors/Compute/Per_ROI_Time_Courses.h"
#include "../YgorImages_Functors/Compute/Contour_Similarity.h"

#include "CT_Liver_Perfusion_Pharmaco.h"


std::list<OperationArgDoc> OpArgDocCT_Liver_Perfusion_Pharmaco(void){
    std::list<OperationArgDoc> out;

    out.emplace_back();
    out.back().name = "AIFROINameRegex";
    out.back().desc = "Regex for the name of the ROI to use as the AIF. It should generally be a"
                      " major artery near the trunk or near the tissue of interest.";
    out.back().default_val = "Abdominal_Aorta";
    out.back().expected = true;
    out.back().examples = { "Abdominal_Aorta",
                            ".*Aorta.*",
                            "Major_Artery" };

    out.emplace_back();
    out.back().name = "PlotAIFVIF";
    out.back().desc = "Control whether the AIF and VIF should be shown prior to modeling.";
    out.back().default_val = "false";
    out.back().expected = true;
    out.back().examples = { "true",
                            "false" };

    out.emplace_back();
    out.back().name = "PlotPixelModel";
    out.back().desc = "Show a plot of the fitted model for a specified pixel. Plotting happens "
                      " immediately after the pixel is processed. You can supply arbitrary"
                      " metadata, but must also supply Row and Column numbers. Note that numerical "
                      " comparisons are performed lexically, so you have to be exact. Also note the"
                      " sub-separation token is a semi-colon, not a colon.";
    out.back().default_val = "";
    out.back().expected = true;
    out.back().examples = { "Row@12;Column@4;Description@.*k1A.*",
                            "Row@256;Column@500;SliceLocation@23;SliceThickness@0.5",
                            "Row@256;Column@500;Some@thing#Row@256;Column@501;Another@thing",
                            "Row@0;Column@5#Row@4;Column@5#Row@8;Column@5#Row@12;Column@5"};

    out.emplace_back();
    out.back().name = "PreDecimateOutSizeR";
    out.back().desc = "The number of pixels along the row unit vector to group into an outgoing pixel."
                      " This optional step can reduce computation effort by downsampling (decimating)"
                      " images before computing fitted parameter maps (but *after* computing AIF and"
                      " VIF time courses)."
                      " Must be a multiplicative factor of the incoming image's row count."
                      " No decimation occurs if either this or 'PreDecimateOutSizeC' is zero or negative.";
    out.back().default_val = "8";
    out.back().expected = true;
    out.back().examples = { "0", "2", "4", "8", "16", "32", "64", "128", "256", "512" };

    out.emplace_back();
    out.back().name = "PreDecimateOutSizeC";
    out.back().desc = "The number of pixels along the column unit vector to group into an outgoing pixel."
                      " This optional step can reduce computation effort by downsampling (decimating)"
                      " images before computing fitted parameter maps (but *after* computing AIF and"
                      " VIF time courses)."
                      " Must be a multiplicative factor of the incoming image's column count."
                      " No decimation occurs if either this or 'PreDecimateOutSizeR' is zero or negative.";
    out.back().default_val = "8";
    out.back().expected = true;
    out.back().examples = { "0", "2", "4", "8", "16", "32", "64", "128", "256", "512" };

    out.emplace_back();
    out.back().name = "TargetROINameRegex";
    out.back().desc = "Regex for the name of the ROI to perform modeling within. The largest contour is"
                      " usually what you want, but you can also be more focused.";
    out.back().default_val = ".*Body.*";
    out.back().expected = true;
    out.back().examples = { "Liver_Patches_For_Testing_Smaller",
                            "Liver_Patches_For_Testing",
                            "Suspected_Liver_Rough",
                            "Rough_Body",
                            ".*body.*",
                            ".*something.*\\|.*another.*thing.*" };
                            
    out.emplace_back();
    out.back().name = "VIFROINameRegex";
    out.back().desc = "Regex for the name of the ROI to use as the VIF. It should generally be a"
                      " major vein near the trunk or near the tissue of interest.";
    out.back().default_val = "Hepatic_Portal_Vein";
    out.back().expected = true;
    out.back().examples = { "Hepatic_Portal_Vein",
                            ".*Portal.*Vein.*",
                            "Major_Vein" };

    return out;
}



Drover CT_Liver_Perfusion_Pharmaco(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> InvocationMetadata, std::string /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto AIFROIName = OptArgs.getValueStr("AIFROINameRegex").value();
    const auto PlotAIFVIF = OptArgs.getValueStr("PlotAIFVIF").value();
    const auto PlotPixelModel = OptArgs.getValueStr("PlotPixelModel").value();
    const long int PreDecimateR = std::stol( OptArgs.getValueStr("PreDecimateOutSizeR").value() );
    const long int PreDecimateC = std::stol( OptArgs.getValueStr("PreDecimateOutSizeC").value() );
    const auto TargetROIName = OptArgs.getValueStr("TargetROINameRegex").value();
    const auto VIFROIName = OptArgs.getValueStr("VIFROINameRegex").value();
    //-----------------------------------------------------------------------------------------------------------------
    const auto AIFROINameRegex = std::regex(AIFROIName, std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto VIFROINameRegex = std::regex(VIFROIName, std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto TargetROINameRegex = std::regex(TargetROIName, std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto PlotAIFVIFRegex = std::regex("tr?u?e?", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    const auto ShouldPlotAIFVIF = std::regex_match(PlotAIFVIF, PlotAIFVIFRegex);


    //Tokenize the plotting criteria.
    std::list<LiverPharmacoModel5ParamChebyUserDataPixelSelectionCriteria> pixels_to_plot;
    const auto RowRegex = std::regex("row", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto ColRegex = std::regex("column", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    for(auto a : SplitStringToVector(PlotPixelModel, '#', 'd')){
        pixels_to_plot.emplace_back();
        pixels_to_plot.back().row = -1;
        pixels_to_plot.back().column = -1;
        for(auto b : SplitStringToVector(a, ';', 'd')){
            auto c = SplitStringToVector(b, '@', 'd');
            if(c.size() != 2) throw std::runtime_error("Cannot parse subexpression: "_s + b);

            if(std::regex_match(c.front(),RowRegex)){
                pixels_to_plot.back().row = std::stol(c.back());
            }else if(std::regex_match(c.front(),ColRegex)){
                pixels_to_plot.back().column = std::stol(c.back());
            }else{
                pixels_to_plot.back().metadata_criteria.insert( { c.front(), 
                    std::regex(c.back(), std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended) } );
            }
        }
    }



    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    std::list<std::reference_wrapper<contour_collection<double>>> cc_all;
    for(auto & cc : DICOM_data.contour_data->ccs){
        auto base_ptr = reinterpret_cast<contour_collection<double> *>(&cc);
        cc_all.push_back( std::ref(*base_ptr) );
    }

    //Get handles for each of the original image arrays so we can easily refer to them later.
    std::vector<std::shared_ptr<Image_Array>> orig_img_arrays;
    for(auto & img_arr : DICOM_data.image_data) orig_img_arrays.push_back(img_arr);


    //Force the window to something reasonable to be uniform and cover normal tissue HU range.
    if(true) for(auto & img_arr : orig_img_arrays){
        if(!img_arr->imagecoll.Process_Images_Parallel( GroupIndividualImages,
                                               StandardAbdominalHUWindow,
                                               {}, {} )){
            FUNCERR("Unable to force window to cover reasonable HU range");
        }
    }

    //Look for relevant invocation metadata.
    double ContrastInjectionLeadTime = 6.0; //Seconds. 
    if(InvocationMetadata.count("ContrastInjectionLeadTime") == 0){
        FUNCWARN("Unable to locate 'ContrastInjectionLeadTime' invocation metadata key. Assuming the default lead time "
                 << ContrastInjectionLeadTime << "s is appropriate");
    }else{
        ContrastInjectionLeadTime = std::stod( InvocationMetadata["ContrastInjectionLeadTime"] );
        if(ContrastInjectionLeadTime < 0.0) throw std::runtime_error("Non-sensical 'ContrastInjectionLeadTime' found.");
        FUNCINFO("Found 'ContrastInjectionLeadTime' invocation metadata key. Using value " << ContrastInjectionLeadTime << "s");
    }

    double ContrastInjectionWashoutTime = 60.0; //Seconds.
    if(InvocationMetadata.count("ContrastInjectionWashoutTime") == 0){
        FUNCWARN("Unable to locate 'ContrastInjectionWashoutTime' invocation metadata key. Assuming the default lead time "
                 << ContrastInjectionWashoutTime << "s is appropriate");
    }else{
        ContrastInjectionWashoutTime = std::stod( InvocationMetadata["ContrastInjectionWashoutTime"] );
        if(ContrastInjectionWashoutTime < 0.0) throw std::runtime_error("Non-sensical 'ContrastInjectionWashoutTime' found.");
        FUNCINFO("Found 'ContrastInjectionWashoutTime' invocation metadata key. Using value " << ContrastInjectionWashoutTime << "s");
    }

    //Whitelist contours. Also rename the remaining into either "AIF" or "VIF".
    auto cc_AIF_VIF = cc_all;
    cc_AIF_VIF.remove_if([=](std::reference_wrapper<contour_collection<double>> cc) -> bool {
                   const auto ROINameOpt = cc.get().contours.front().GetMetadataValueAs<std::string>("ROIName");
                   if(!ROINameOpt) return true; //Remove those without names.
                   const auto ROIName = ROINameOpt.value();
                   const auto MatchesAIF = std::regex_match(ROIName,AIFROINameRegex);
                   const auto MatchesVIF = std::regex_match(ROIName,VIFROINameRegex);
                   if(!MatchesAIF && !MatchesVIF) return true;

                   //Keep them, but rename them all.
                   for(auto &acontour : cc.get().contours){
                       acontour.metadata["ROIName"] = MatchesAIF ? "AIF" : "VIF";
                   }
                   return false;
    });


    //Compute a baseline with which we can use later to compute signal enhancement.
    std::vector<std::shared_ptr<Image_Array>> baseline_img_arrays;
    if(true){
        //Baseline = temporally averaged pre-contrast-injection signal.

        auto PurgeAboveNSeconds = std::bind(PurgeAboveTemporalThreshold, std::placeholders::_1, ContrastInjectionLeadTime);

        for(auto & img_arr : orig_img_arrays){
            DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr ) );
            baseline_img_arrays.emplace_back( DICOM_data.image_data.back() );
    
            baseline_img_arrays.back()->imagecoll.Prune_Images_Satisfying(PurgeAboveNSeconds);
    
            if(!baseline_img_arrays.back()->imagecoll.Condense_Average_Images(GroupSpatiallyOverlappingImages)){
                FUNCERR("Cannot temporally average data set. Is it able to be averaged?");
            }
        }
    
    }else{
        //Baseline = minimum of signal over whole time course (minimum is usually pre-contrast, but noise
        // can affect the result).

        for(auto & img_arr : orig_img_arrays){
            DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr ) );
            baseline_img_arrays.emplace_back( DICOM_data.image_data.back() );
    
            if(!baseline_img_arrays.back()->imagecoll.Process_Images_Parallel( GroupSpatiallyOverlappingImages,
                                                                      CondenseMinPixel,
                                                                      {}, {} )){
                FUNCERR("Unable to generate min(pixel) images over the time course");
            }
        }
    }


    //Deep-copy the original long image array and use the baseline map to work out 
    // approximate contrast enhancement in each voxel.
    std::vector<std::shared_ptr<Image_Array>> C_enhancement_img_arrays;
    {
        auto img_arr = orig_img_arrays.front();
        DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr ) );
        C_enhancement_img_arrays.emplace_back( DICOM_data.image_data.back() );

        if(!C_enhancement_img_arrays.back()->imagecoll.Transform_Images( CTPerfusionSigDiffC,
                                                                         { baseline_img_arrays.front()->imagecoll },
                                                                         { } )){
            FUNCERR("Unable to transform image array to make poor-man's C map");
        }
    }
       
    //Eliminate some images to relieve some memory pressure.
    if(true){
        for(auto & img_arr : orig_img_arrays){
            img_arr->imagecoll.images.clear();
        }
        for(auto & img_arr : baseline_img_arrays){
            img_arr->imagecoll.images.clear();
        }
    }
 
    //Compute some aggregate C(t) curves from the available ROIs. We especially want the 
    // portal vein and ascending aorta curves.
    ComputePerROITimeCoursesUserData ud; // User Data.
    if(true) for(auto & img_arr : C_enhancement_img_arrays){
        if(!img_arr->imagecoll.Compute_Images( ComputePerROICourses,   //Non-modifying function, can use in-place.
                                               { },
                                               cc_AIF_VIF,
                                               &ud )){
            FUNCERR("Unable to compute per-ROI time courses");
        }
    }
    //For perfusion purposes, we always want to scale down the ROIs per-atomos (i.e., per-voxel).
    for(auto & tcs : ud.time_courses){
        const auto lROIname = tcs.first;
        const auto lVoxelCount = ud.voxel_count[lROIname];
        tcs.second = tcs.second.Multiply_With(1.0/static_cast<double>(lVoxelCount));
    }

    //Scale the contrast agent to account for the fact that contrast agent does not enter the RBCs.
    //
    // NOTE: "Because the contrast agent does not enter the RBCs, the time series Caorta(t) and Cportal(t)
    //        were divided by one minus the hematocrit." (From Van Beers et al. 2000.)
    for(auto & theROI : ud.time_courses){
        theROI.second = theROI.second.Multiply_With(1.0/(1.0 - 0.42));
    }


    //Decimate the number of pixels for modeling purposes.
    if((PreDecimateR > 0) && (PreDecimateC > 0)){
        auto DecimateRC = std::bind(InImagePlanePixelDecimate, 
                                    std::placeholders::_1, std::placeholders::_2, 
                                    std::placeholders::_3, std::placeholders::_4,
                                    PreDecimateR, PreDecimateC,
                                    std::experimental::any());


        //for(auto & img_arr : DICOM_data.image_data){
        for(auto & img_arr : C_enhancement_img_arrays){
            if(!img_arr->imagecoll.Process_Images_Parallel( GroupIndividualImages,
                                                   DecimateRC,
                                                   {}, {} )){
                FUNCERR("Unable to decimate pixels");
            }
        }
    }

    //Using the ROI time curves, compute a pharmacokinetic model and produce an image map 
    // with some model parameter(s).
    std::vector<std::shared_ptr<Image_Array>> pharmaco_model_dummy; //This gets destroyed ASAP after computation.
    std::vector<std::shared_ptr<Image_Array>> pharmaco_model_kA;
    std::vector<std::shared_ptr<Image_Array>> pharmaco_model_tauA;
    std::vector<std::shared_ptr<Image_Array>> pharmaco_model_kV;
    std::vector<std::shared_ptr<Image_Array>> pharmaco_model_tauV;
    std::vector<std::shared_ptr<Image_Array>> pharmaco_model_k2;


    //Prune images, to reduce the computational effort needed.
    if(false){
        for(auto & img_arr : C_enhancement_img_arrays){
            const auto centre = img_arr->imagecoll.center();
            img_arr->imagecoll.Retain_Images_Satisfying(
                                  [=](const planar_image<float,double> &animg)->bool{
                                      return animg.encompasses_point(centre);
                                  });
        }
    }


    //Use a linear model.
    if(false){ 
        for(auto & img_arr : C_enhancement_img_arrays){
            DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr ) );
            pharmaco_model_dummy.emplace_back( DICOM_data.image_data.back() );

            DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( ) );
            pharmaco_model_kA.emplace_back( DICOM_data.image_data.back() );
            DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( ) );
            pharmaco_model_tauA.emplace_back( DICOM_data.image_data.back() );
            DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( ) );
            pharmaco_model_kV.emplace_back( DICOM_data.image_data.back() );
            DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( ) );
            pharmaco_model_tauV.emplace_back( DICOM_data.image_data.back() );
            DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( ) );
            pharmaco_model_k2.emplace_back( DICOM_data.image_data.back() );

            if(!pharmaco_model_dummy.back()->imagecoll.Process_Images_Parallel( 
                              GroupSpatiallyOverlappingImages,
                              LiverPharmacoModel5ParamLinear,
                              { std::ref(pharmaco_model_kA.back()->imagecoll),
                                std::ref(pharmaco_model_tauA.back()->imagecoll),
                                std::ref(pharmaco_model_kV.back()->imagecoll),
                                std::ref(pharmaco_model_tauV.back()->imagecoll),
                                std::ref(pharmaco_model_k2.back()->imagecoll) }, 
                              cc_all,
                              &ud )){
                FUNCERR("Unable to pharmacokinetically model liver!");
            }else{
                pharmaco_model_dummy.back()->imagecoll.images.clear();
            }
        }
        pharmaco_model_dummy.clear();

    //Use a Chebyshev model.
    }else{
        decltype(ud.time_courses) orig_time_courses;
        for(const auto & tc : ud.time_courses) orig_time_courses["Original " + tc.first] = tc.second;

        //Pre-process the AIF and VIF time courses.
        LiverPharmacoModel5ParamChebyUserData ud2; 

        ud2.pixels_to_plot = pixels_to_plot;
        ud2.TargetROIs = TargetROINameRegex;
        ud2.ContrastInjectionLeadTime = ContrastInjectionLeadTime;
        {
            //Correct any unaccounted-for contrast enhancement shifts. 
            if(true) for(auto & theROI : ud.time_courses){
                //Subtract the minimum over the full time course.
                if(false){
                    const auto Cmin = theROI.second.Get_Extreme_Datum_y().first;
                    theROI.second = theROI.second.Sum_With(0.0-Cmin[2]);

                //Subtract the mean from the pre-injection period.
                }else{
                    const auto preinject = theROI.second.Select_Those_Within_Inc(-1E99,ContrastInjectionLeadTime);
                    const auto themean = preinject.Mean_y()[0];
                    theROI.second = theROI.second.Sum_With(0.0-themean);
                }
            }
        
            //Insert some virtual points before the first sample (assumed to be at t=0).
            //
            // Note: If B-splines are used you need to have good coverage. If linear interpolation is used you only
            //       need two (one the the far left and one near t=0).
            if(true) for(auto & theROI : ud.time_courses){
                theROI.second.push_back( -25.0, 0.0, 0.0, 0.0 );
                theROI.second.push_back( -20.0, 0.0, 0.0, 0.0 );
                theROI.second.push_back( -17.0, 0.0, 0.0, 0.0 );
                theROI.second.push_back( -13.0, 0.0, 0.0, 0.0 );
                theROI.second.push_back(  -9.0, 0.0, 0.0, 0.0 );
                theROI.second.push_back(  -5.0, 0.0, 0.0, 0.0 );
                theROI.second.push_back(  -2.0, 0.0, 0.0, 0.0 );
                theROI.second.push_back(  -1.0, 0.0, 0.0, 0.0 );
            }
        
            //Perform smoothing on the AIF and VIF to help reduce optimizer bounce.
            if(false) for(auto & theROI : ud.time_courses){
                theROI.second = theROI.second.Resample_Equal_Spacing(200);
                theROI.second = theROI.second.Moving_Median_Filter_Two_Sided_Equal_Weighting(2);
            }
       
            //Extrapolate beyond the data collection limit (to stop the optimizer getting snagged
            // on any sharp drop-offs when shifting tauA and tauV).
            if(true) for(auto & theROI : ud.time_courses){
                const auto washout = theROI.second.Select_Those_Within_Inc(ContrastInjectionWashoutTime,1E99);
                const auto leastsquares =  washout.Linear_Least_Squares_Regression();
                const auto tmax = theROI.second.Get_Extreme_Datum_x().second[0];
                const auto virtdatum_t = tmax + 25.0;
                const auto virtdatum_f = leastsquares.evaluate_simple(virtdatum_t);
                theROI.second.push_back(virtdatum_t, 0.0, virtdatum_f, 0.0 );
            }

            //Perform smoothing on the AIF and VIF using NPLLR.
            if(false) for(auto & theROI : ud.time_courses){
                bool lOK;
                orig_time_courses["NPLLR: " + theROI.first] = NPRLL::Attempt_Auto_Smooth(theROI.second, &lOK);
                theROI.second = NPRLL::Attempt_Auto_Smooth(theROI.second, &lOK);
                if(!lOK) throw std::runtime_error("Unable to smooth AIF or VIF.");
            }

            //Approximate the VIF and VIF with a Chebyshev polynomial approximation.
            if(true){
                //Use basis spline interpolation.
                for(auto & theROI : ud.time_courses){
                    const auto num_bs_coeffs = theROI.second.size() / 2; // Number of B-spline coefficients (to fit).
                    const auto num_ca_coeffs = theROI.second.size() * 2; // Number of Chebyshev poly coeffs (to compute).


                    theROI.second = theROI.second.Strip_Uncertainties_in_y();

                    const auto tmin = theROI.second.Get_Extreme_Datum_x().first[0];
                    const auto tmax = theROI.second.Get_Extreme_Datum_x().second[0];
                    const auto pinf = std::numeric_limits<double>::infinity(); //use automatic (maximal) endpoint determination.

                    //basis_spline bs(theROI.second, pinf, pinf, 4, num_bs_coeffs, basis_spline_breakpoints::equal_spacing);
                    basis_spline bs(theROI.second, pinf, pinf, 4, num_bs_coeffs, basis_spline_breakpoints::adaptive_datum_density);
                    auto interp = [&bs](double t) -> double {
                            const auto interpolated_f = bs.Sample(t)[2];
                            return interpolated_f;
                    };
                    cheby_approx<double> ca;
                    ca.Prepare(interp, num_ca_coeffs, tmin + 5.0, tmax - 5.0);

                    ud2.time_courses[theROI.first] = ca;
                    ud2.time_course_derivatives[theROI.first] = ca.Chebyshev_Derivative();
                }
            }else{
                //Use (default) linear interpolation.
                for(auto & theROI : ud.time_courses){
                    const auto num_ca_coeffs = theROI.second.size() * 2; // Number of Chebyshev poly coeffs (to compute).

                    const auto tmin = theROI.second.Get_Extreme_Datum_x().first[0];
                    const auto tmax = theROI.second.Get_Extreme_Datum_x().second[0];

                    cheby_approx<double> ca;
                    ca.Prepare(theROI.second, num_ca_coeffs, tmin + 5.0, tmax - 5.0);

                    ud2.time_courses[theROI.first] = ca;
                    ud2.time_course_derivatives[theROI.first] = ca.Chebyshev_Derivative();
                }
            }

            if(ShouldPlotAIFVIF){
                PlotTimeCourses("Processed AIF and VIF", orig_time_courses, ud2.time_courses);
            }
        }

        for(auto & img_arr : C_enhancement_img_arrays){
            DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr ) );
            pharmaco_model_dummy.emplace_back( DICOM_data.image_data.back() );

            DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( ) );
            pharmaco_model_kA.emplace_back( DICOM_data.image_data.back() );
            DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( ) );
            pharmaco_model_tauA.emplace_back( DICOM_data.image_data.back() );
            DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( ) );
            pharmaco_model_kV.emplace_back( DICOM_data.image_data.back() );
            DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( ) );
            pharmaco_model_tauV.emplace_back( DICOM_data.image_data.back() );
            DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( ) );
            pharmaco_model_k2.emplace_back( DICOM_data.image_data.back() );

            if(!pharmaco_model_dummy.back()->imagecoll.Process_Images_Parallel( 
                              GroupSpatiallyOverlappingImages,
                              LiverPharmacoModel5ParamCheby,
                              { std::ref(pharmaco_model_kA.back()->imagecoll),
                                std::ref(pharmaco_model_tauA.back()->imagecoll),
                                std::ref(pharmaco_model_kV.back()->imagecoll),
                                std::ref(pharmaco_model_tauV.back()->imagecoll),
                                std::ref(pharmaco_model_k2.back()->imagecoll) }, 
                              cc_all,
                              &ud2 )){
                FUNCERR("Unable to pharmacokinetically model liver!");
            }else{
                pharmaco_model_dummy.back()->imagecoll.images.clear();
            }
        }
        pharmaco_model_dummy.clear();
    }


    //Ensure the images are properly spatially ordered.
    if(true){
        for(auto & img_array : DICOM_data.image_data){
            //img_array->imagecoll.Stable_Sort_on_Metadata_Keys_Value_Numeric<long int>("InstanceNumber");
            img_array->imagecoll.Stable_Sort_on_Metadata_Keys_Value_Numeric<double>("SliceLocation");
            img_array->imagecoll.Stable_Sort_on_Metadata_Keys_Value_Numeric<double>("dt");
        }
    }
    
    return std::move(DICOM_data);
}
