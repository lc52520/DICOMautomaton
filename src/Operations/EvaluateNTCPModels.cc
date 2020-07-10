//EvaluateNTCPModels.cc - A part of DICOMautomaton 2017. Written by hal clark.

#include <boost/interprocess/creation_tags.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <cmath>
#include <exception>
#include <any>
#include <optional>
#include <fstream>
#include <functional>
#include <iostream>
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
#include "../YgorImages_Functors/Compute/AccumulatePixelDistributions.h"
#include "EvaluateNTCPModels.h"
#include "Explicator.h"       //Needed for Explicator class.
#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorStats.h"        //Needed for Stats:: namespace.



OperationDoc OpArgDocEvaluateNTCPModels(){
    OperationDoc out;
    out.name = "EvaluateNTCPModels";

    out.desc = 
        "This operation evaluates a variety of NTCP models for each provided ROI. The selected ROI should be OARs."
        " Currently the following are implemented:"
        " (1) The LKB model."
        " (2) The 'Fenwick' model for solid tumours (in the lung; for a whole-lung OAR).";
        //" (3) Forthcoming: modified Equivalent Uniform Dose (mEUD) NTCP model.";
        
    out.notes.emplace_back(
        "Generally these models require dose in 2 Gy per fraction equivalents ('EQD2'). You must pre-convert the data"
        " if the RT plan is not already 2 Gy per fraction. There is no easy way to ensure this conversion has taken place"
        " or was unnecessary."
    );
        
    out.notes.emplace_back(
        "This routine will combine spatially-overlapping images by summing voxel intensities. So if you have a time"
        " course it may be more sensible to aggregate images in some way (e.g., spatial averaging) prior to calling"
        " this routine."
    );
        
    out.notes.emplace_back(
        "The LKB and mEUD both have their own gEUD 'alpha' parameter, but they are not necessarily shared."
        " Huang et al. 2015 (doi:10.1038/srep18010) used alpha=1 for the LKB model and alpha=5 for the mEUD model."
    );


    out.args.emplace_back();
    out.args.back().name = "NTCPFileName";
    out.args.back().desc = "A filename (or full path) in which to append NTCP data generated by this routine."
                      " The format is CSV. Leave empty to dump to generate a unique temporary file.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "", "/tmp/somefile", "localfile.csv", "derivative_data.csv" };
    out.args.back().mimetype = "text/csv";


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


/*    
    out.args.emplace_back();
    out.args.back().name = "Gamma50";
    out.args.back().desc = "The unitless 'normalized dose-response gradient' or normalized slope of the logistic"
                      " dose-response model at the half-maximum point (e.g., D_50). Informally,"
                      " this parameter controls the steepness of the dose-response curve. (For more"
                      " specific information, consult a standard reference such as 'Basic Clinical Radiobiology'"
                      " 4th Edition by Joiner et al., sections 5.3-5.5.) This parameter is empirically"
                      " fit and not universal. Late endpoints for normal tissues have gamma_50 around 2-6"
                      " whereas gamma_50 nominally varies around 1.5-2.5 for local control of squamous"
                      " cell carcinomas of the head and neck.";
    out.args.back().default_val = "2.3";
    out.args.back().expected = true;
    out.args.back().examples = { "1.5", "2", "2.5", "6" };

    
    out.args.emplace_back();
    out.args.back().name = "Dose50";
    out.args.back().desc = "The dose (in Gray) needed to achieve 50\% probability of local tumour control according to"
                      " an empirical logistic dose-response model (e.g., D_50). Informally, this"
                      " parameter 'shifts' the model along the dose axis. (For more"
                      " specific information, consult a standard reference such as 'Basic Clinical Radiobiology'"
                      " 4th Edition by Joiner et al., sections 5.1-5.3.) This parameter is empirically"
                      " fit and not universal. In 'Quantifying the position and steepness of radiation "
                      " dose-response curves' by Bentzen and Tucker in 1994, D_50 of around 60-65 Gy are reported"
                      " for local control of head and neck cancers (pyriform sinus carcinoma and neck nodes with"
                      " max diameter <= 3cm). Martel et al. report 84.5 Gy in lung.";
    out.args.back().default_val = "65";
    out.args.back().expected = true;
    out.args.back().examples = { "37.9", "52", "60", "65", "84.5" };


    out.args.emplace_back();
    out.args.back().name = "EUD_Gamma50";
    out.args.back().desc = "The unitless 'normalized dose-response gradient' or normalized slope of the gEUD TCP model."
                      " It is defined only for the generalized Equivalent Uniform Dose (gEUD) model."
                      " This is sometimes referred to as the change in TCP for a unit change in dose straddled at"
                      " the TCD_50 dose. It is a counterpart to the Martel model's 'Gamma_50' parameter, but is"
                      " not quite the same."
                      " Okunieff et al. (doi:10.1016/0360-3016(94)00475-Z) computed Gamma50 for tumours in human"
                      " subjects across multiple institutions; they found a median of 0.8 for gross disease and"
                      " a median of 1.5 for microscopic disease. The inter-quartile range was [0.7:1.8] and"
                      " [0.7:2.2] respectively. (Refer to table 3 for site-specific values.) Additionally, "
                      " Gay et al. (doi:10.1016/j.ejmp.2007.07.001) claim that a value of 4.0 for late effects"
                      " a value of 2.0 for tumors in 'are reasonable initial estimates in [our] experience.' Their"
                      " table 2 lists (NTCP) estimates based on the work of Emami (doi:10.1016/0360-3016(91)90171-Y).";
    out.args.back().default_val = "0.8";
    out.args.back().expected = true;
    out.args.back().examples = { "0.8", "1.5" };
*/


    out.args.emplace_back();
    out.args.back().name = "LKB_TD50";
    out.args.back().desc = "The dose (in Gray) needed to deliver to the selected OAR that will induce the effect in 50%"
                           " of cases.";
    out.args.back().default_val = "26.8";
    out.args.back().expected = true;
    out.args.back().examples = { "26.8" };


    out.args.emplace_back();
    out.args.back().name = "LKB_M";
    out.args.back().desc = "No description given...";
    out.args.back().default_val = "0.45";
    out.args.back().expected = true;
    out.args.back().examples = { "0.45" };


    out.args.emplace_back();
    out.args.back().name = "LKB_Alpha";
    out.args.back().desc = "The weighting factor $\\alpha$ that controls the relative weighting of volume and dose"
                      " in the generalized Equivalent Uniform Dose (gEUD) model."
                      " When $\\alpha=1$, the gEUD is equivalent to the mean; when $\\alpha=0$, the gEUD is equivalent to"
                      " the geometric mean."
                      " Wu et al. (doi:10.1016/S0360-3016(01)02585-8) claim that for normal tissues, $\\alpha$ can be"
                      " related to the Lyman-Kutcher-Burman (LKB) model volume parameter 'n' via $\\alpha=1/n$."
                      " Sovik et al. (doi:10.1016/j.ejmp.2007.09.001) found that gEUD is not strongly impacted by"
                      " errors in $\\alpha$."
                      " Niemierko et al. ('A generalized concept of equivalent uniform dose. Med Phys 26:1100, 1999)"
                      " generated maximum likelihood estimates for 'several tumors and normal structures' which"
                      " ranged from -13.1 for local control of chordoma tumors to +17.7 for perforation of "
                      " esophagus." 
                      " Gay et al. (doi:10.1016/j.ejmp.2007.07.001) table 2 lists estimates based on the"
                      " work of Emami (doi:10.1016/0360-3016(91)90171-Y) for normal tissues ranging from 1-31."
                      " Brenner et al. (doi:10.1016/0360-3016(93)90189-3) recommend -7.2 for breast cancer, "
                      " -10 for melanoma, and -13 for squamous cell carcinomas. A 2017 presentation by Ontida "
                      " Apinorasethkul claims the tumour range spans [-40:-1] and the organs at risk range "
                      " spans [1:40]. AAPM TG report 166 also provides a listing of recommended values,"
                      " suggesting -10 for PTV and GTV, +1 for parotid, 20 for spinal cord, and 8-16 for"
                      " rectum, bladder, brainstem, chiasm, eye, and optic nerve. Burman (1991) and QUANTEC"
                      " (2010) also provide estimates.";
    out.args.back().default_val = "1.0";
    out.args.back().expected = true;
    out.args.back().examples = { "1", "3", "4", "20", "31" };


    out.args.emplace_back();
    out.args.back().name = "UserComment";
    out.args.back().desc = "A string that will be inserted into the output file which will simplify merging output"
                      " with differing parameters, from different sources, or using sub-selections of the data."
                      " If left empty, the column will be omitted from the output.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "", "Using XYZ", "Patient treatment plan C" };

    return out;
}



Drover EvaluateNTCPModels(Drover DICOM_data, const OperationArgPkg& OptArgs, const std::map<std::string,std::string>& /*InvocationMetadata*/, const std::string& FilenameLex){

    //---------------------------------------------- User Parameters --------------------------------------------------
    auto NTCPFileName = OptArgs.getValueStr("NTCPFileName").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();

    const auto LKB_M = std::stod( OptArgs.getValueStr("LKB_M").value() );
    const auto LKB_TD50 = std::stod( OptArgs.getValueStr("LKB_TD50").value() );
    const auto LKB_Alpha = std::stod( OptArgs.getValueStr("LKB_Alpha").value() );

    const auto UserComment = OptArgs.getValueStr("UserComment");

/*
    const auto Gamma50 = std::stod( OptArgs.getValueStr("Gamma50").value() );
    const auto Dose50 = std::stod( OptArgs.getValueStr("Dose50").value() );

    const auto EUD_Gamma50 = std::stod( OptArgs.getValueStr("EUD_Gamma50").value() );
    const auto EUD_TCD50 = std::stod( OptArgs.getValueStr("EUD_TCD50").value() );
    const auto EUD_Alpha = std::stod( OptArgs.getValueStr("EUD_Alpha").value() );
*/

    //-----------------------------------------------------------------------------------------------------------------

    Explicator X(FilenameLex);

    //Merge the image arrays if necessary.
    if(DICOM_data.image_data.empty()){
        throw std::invalid_argument("This routine requires at least one image array. Cannot continue");
    }

    auto img_arr_ptr = DICOM_data.image_data.front();
    if(img_arr_ptr == nullptr){
        throw std::runtime_error("Encountered a nullptr when expecting a valid Image_Array ptr.");
    }else if(img_arr_ptr->imagecoll.images.empty()){
        throw std::runtime_error("Encountered a Image_Array with valid images -- no images found.");
    }

    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    auto cc_all = All_CCs( DICOM_data );
    auto cc_ROIs = Whitelist( cc_all, { { "ROIName", ROILabelRegex },
                                        { "NormalizedROIName", NormalizedROILabelRegex } } );

    if(cc_ROIs.empty()){
        throw std::invalid_argument("No contours selected. Cannot continue.");
    }

    std::string patient_ID;
    if( auto o = cc_ROIs.front().get().contours.front().GetMetadataValueAs<std::string>("PatientID") ){
        patient_ID = o.value();
    }else if( auto o = cc_ROIs.front().get().contours.front().GetMetadataValueAs<std::string>("StudyInstanceUID") ){
        patient_ID = o.value();
    }else{
        patient_ID = "unknown_patient";
    }

    //Accumulate the voxel intensity distributions.
    AccumulatePixelDistributionsUserData ud;
    if(!img_arr_ptr->imagecoll.Compute_Images( AccumulatePixelDistributions, { },
                                               cc_ROIs, &ud )){
        throw std::runtime_error("Unable to accumulate pixel distributions.");
    }

    //Evalute the models.
    std::map<std::string, double> LKBModel;
    std::map<std::string, double> FenwickModel;
//    std::map<std::string, double> mEUDModel;
    {
        for(const auto &av : ud.accumulated_voxels){
            const auto lROIname = av.first;

            const auto N = av.second.size();
            const long double V_frac = static_cast<long double>(1) / N; // Fractional volume of a single voxel compared to whole ROI.

            std::vector<double> LKB_gEUD_elements;
//            std::vector<double> mEUD_elements;
            LKB_gEUD_elements.reserve(N);
//            mEUD_elements.reserve(N);
            for(const auto &D_voxel : av.second){
                // LKB model.
                //
                // Note: Assumes voxel doses are EQD2. Pre-convert if the RT plan is not already in 2Gy/fraction!
                {
                    const auto scaled = std::pow(D_voxel, LKB_Alpha); //Problematic for (non-physical) 0.0.
                    if(std::isfinite(scaled)){
                        LKB_gEUD_elements.push_back(V_frac * std::pow(D_voxel, LKB_Alpha));
                    }else{
                        LKB_gEUD_elements.push_back(0.0); //No real need to include this except acknowledging what we are doing.
                    }
                }
                
                // mEUD model.
                //
                // Note: Assumes voxel doses are EQD2. Pre-convert if the RT plan is not already in 2Gy/fraction!
                {
//NOTE: this model only uses the 100c with the highest dose. So sort and filter the voxels before computing mEUD!
// Also, the model presented by Huang et al. is underspecified in their paper. Check the original for more comprehensive
// explanation.
//                    gEUD_elements.push_back(V_frac * std::pow(D_voxel, gEUD_Alpha));
                }

                // ... other models ...
                // ...

            }

            //Post-processing.
            {
                const auto OAR_mean_dose = Stats::Mean(av.second);
                const auto numer = OAR_mean_dose - 29.2;
                const auto denom = 13.1 * std::sqrt(2);
                const auto t = numer/denom;
                const auto NTCP_Fenwick = 0.5*(1.0 + std::erf(t));
                FenwickModel[lROIname] = NTCP_Fenwick;
            }
            {
                const long double LKB_gEUD = std::pow( Stats::Sum(LKB_gEUD_elements), static_cast<long double>(1) / LKB_Alpha );

                const long double numer = LKB_gEUD - LKB_TD50;
                const long double denom = LKB_M * LKB_TD50 * std::sqrt(2.0);
                const long double t = numer/denom;
                const long double NTCP_LKB = 0.5*(1.0 + std::erf(t));
                LKBModel[lROIname] = NTCP_LKB;
            }
            {
/*
                const long double mEUD = std::pow( Stats::Sum(mEUD_elements), static_cast<long double>(1) / EUD_Alpha );

                const long double numer = std::pow(mEUD, EUD_Gamma50*4);
                const long double denom = numer + std::pow(EUD_TCD50, EUD_Gamma50*4);
                double NTCP_mEUD = numer/denom; // This is a sigmoid curve.

                mEUDModel[lROIname] = NTCP_mEUD; 
*/
            }
        }
    }


    //Report the findings. 
    FUNCINFO("Attempting to claim a mutex");
    try{
        //File-based locking is used so this program can be run over many patients concurrently.
        //
        //Try open a named mutex. Probably created in /dev/shm/ if you need to clear it manually...
        boost::interprocess::named_mutex mutex(boost::interprocess::open_or_create,
                                               "dicomautomaton_operation_evaluatentcp_mutex");
        boost::interprocess::scoped_lock<boost::interprocess::named_mutex> lock(mutex);

        if(NTCPFileName.empty()){
            NTCPFileName = Get_Unique_Sequential_Filename("/tmp/dicomautomaton_evaluatentcp_", 6, ".csv");
        }
        const auto FirstWrite = !Does_File_Exist_And_Can_Be_Read(NTCPFileName);
        std::fstream FO_tcp(NTCPFileName, std::fstream::out | std::fstream::app);
        if(!FO_tcp){
            throw std::runtime_error("Unable to open file for reporting derivative data. Cannot continue.");
        }
        if(FirstWrite){ // Write a CSV header.
            FO_tcp << "UserComment,"
                   << "PatientID,"
                   << "ROIname,"
                   << "NormalizedROIname,"
                   << "NTCPLKBModel,"
//                   << "NTCPmEUDModel,"
                   << "NTCPFenwickModel,"
                   << "DoseMin,"
                   << "DoseMean,"
                   << "DoseMedian,"
                   << "DoseMax,"
                   << "DoseStdDev,"
                   << "VoxelCount"
                   << std::endl;
        }
        for(const auto &av : ud.accumulated_voxels){
            const auto lROIname = av.first;
            const auto DoseMin = Stats::Min( av.second );
            const auto DoseMean = Stats::Mean( av.second );
            const auto DoseMedian = Stats::Median( av.second );
            const auto DoseMax = Stats::Max( av.second );
            const auto DoseStdDev = std::sqrt(Stats::Unbiased_Var_Est( av.second ));
            const auto NTCPLKB = LKBModel[lROIname];
//            const auto NTCPmEUD = mEUDModel[lROIname];
            const auto NTCPFenwick = FenwickModel[lROIname];

            FO_tcp  << UserComment.value_or("") << ","
                    << patient_ID        << ","
                    << lROIname          << ","
                    << X(lROIname)       << ","
                    << NTCPLKB*100.0     << ","
//                    << NTCPmEUD*100.0    << ","
                    << NTCPFenwick*100.0 << ","
                    << DoseMin           << ","
                    << DoseMean          << ","
                    << DoseMedian        << ","
                    << DoseMax           << ","
                    << DoseStdDev        << ","
                    << av.second.size()
                    << std::endl;
        }
        FO_tcp.flush();
        FO_tcp.close();

    }catch(const std::exception &e){
        FUNCERR("Unable to write to output NTCP file: '" << e.what() << "'");
    }

    return DICOM_data;
}
