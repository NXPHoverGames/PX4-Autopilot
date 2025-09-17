/****************************************************************************
 *
 *   Copyright (c) 2023 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/**
 * @file VTEPosition.h
 * @brief Estimate the state of a target by processing and fusing sensor data in a Kalman Filter.
 *
 * @author Jonas Perolini <jonspero@me.com>
 *
 */

#pragma once

#include <lib/perf/perf_counter.h>
#include <px4_platform_common/module_params.h>
#include <px4_platform_common/workqueue.h>
#include <drivers/drv_hrt.h>
#include <parameters/param.h>
#include <uORB/Publication.hpp>
#include <uORB/Subscription.hpp>
#include <uORB/SubscriptionInterval.hpp>
#include <uORB/topics/vehicle_acceleration.h>
#include <uORB/topics/vehicle_attitude.h>
#include <uORB/topics/vehicle_local_position.h>
#include <uORB/topics/vehicle_status.h>
#include <uORB/topics/landing_target_pose.h>
#include <uORB/topics/fiducial_marker_pos_report.h>
#include <uORB/topics/target_gnss.h>
#include <uORB/topics/vision_target_est_position.h>
#include <uORB/topics/estimator_sensor_bias.h>
#include <uORB/topics/parameter_update.h>
#include <uORB/topics/estimator_aid_source3d.h>
#include <uORB/topics/vehicle_odometry.h>
#include <uORB/topics/position_setpoint_triplet.h>
#include <uORB/topics/vehicle_land_detected.h>
#include <uORB/topics/sensor_gps.h>
#include <uORB/topics/sensor_uwb.h>
#include <matrix/math.hpp>
#include <mathlib/mathlib.h>
#include <matrix/Matrix.hpp>
#include <lib/conversion/rotation.h>
#include <lib/geo/geo.h>
#include "KF_position.h"
#include <vtest_derivation/generated/state.h>
#include "../common.h"

using namespace time_literals;

namespace vision_target_estimator
{

class VTEPosition: public ModuleParams
{
public:

	VTEPosition();
	virtual ~VTEPosition();

	/*
	 * Get new measurements and update the state estimate
	 */
	void update(const matrix::Vector3f &acc_ned);

	bool init();

	void reset_filter();

	void set_mission_position(const double lat_deg, const double lon_deg, const float alt_m);

	void set_range_sensor(const float dist, const bool valid, const hrt_abstime timestamp);

	void set_local_velocity(const matrix::Vector3f &vel_xyz, const bool valid, const hrt_abstime timestamp);

	void set_local_position(const matrix::Vector3f &xyz, const bool valid, const hrt_abstime timestamp);

	void set_gps_pos_offset(const matrix::Vector3f &xyz, const bool gps_is_offset);

	void set_velocity_offset(const matrix::Vector3f &xyz);

	void set_vte_timeout(const float tout) {_vte_TIMEOUT_US = static_cast<uint32_t>(tout * 1_s);};

	void set_vte_aid_mask(const int mask) {_vte_aid_mask = mask;};

	bool has_timed_out() {return _has_timed_out;};

	// TODO: decide if a relative position measurement is required.
	bool has_fusion_enabled() {return _vte_aid_mask != SensorFusionMask::NO_SENSOR_FUSION;};

private:
	struct accInput {

		bool acc_ned_valid = false;
		matrix::Vector3f vehicle_acc_ned;
	};

protected:

	/*
	 * Update parameters.
	 */
	void updateParams() override;

	// Geographic limits
	static constexpr double lat_abs_max_deg =  90.0;
	static constexpr double lon_abs_max_deg = 180.0;
	static constexpr float alt_min_m = -350.f;
	static constexpr float alt_max_m = 10000.f;
	/* minimum angle for target yaw estimation sqrd*/
	static constexpr float min_angle_for_target_yaw_estimation_sqrd = 0.1f * 0.1f;

	uORB::Publication<landing_target_pose_s> _targetPosePub{ORB_ID(landing_target_pose)};
	uORB::Publication<vision_target_est_position_s> _targetEstimatorStatePub{ORB_ID(vision_target_est_position)};

	// publish innovations target_estimator_gps_pos
	uORB::Publication<estimator_aid_source3d_s> _vte_aid_gps_pos_target_pub{ORB_ID(vte_aid_gps_pos_target)};
	uORB::Publication<estimator_aid_source3d_s> _vte_aid_gps_pos_mission_pub{ORB_ID(vte_aid_gps_pos_mission)};
	uORB::Publication<estimator_aid_source3d_s> _vte_aid_gps_vel_target_pub{ORB_ID(vte_aid_gps_vel_target)};
	uORB::Publication<estimator_aid_source3d_s> _vte_aid_gps_vel_uav_pub{ORB_ID(vte_aid_gps_vel_uav)};
	uORB::Publication<estimator_aid_source3d_s> _vte_aid_fiducial_marker_pub{ORB_ID(vte_aid_fiducial_marker)};
	uORB::Publication<estimator_aid_source3d_s> _vte_aid_uwb_pub{ORB_ID(vte_aid_uwb)};

	uORB::SubscriptionInterval _parameter_update_sub{ORB_ID(parameter_update), 1_s};

private:

	enum Axis {
		X,
		Y,
		Z,
		Count,
	};

	bool _has_timed_out{false};

	enum ObsType {
		Target_gps_pos,
		Mission_gps_pos,
		Uav_gps_vel,
		Target_gps_vel,
		Fiducial_marker,
		Uwb,
		Type_count
	};

	struct targetObs {

		ObsType type;
		hrt_abstime timestamp = 0;

		bool updated; // Indicates if we observations were updated. Only one value for x,y,z directions to reduce stack size.
		matrix::Vector3f meas_xyz{};			// Measurements (meas_x, meas_y, meas_z)
		matrix::Vector3f meas_unc_xyz{};		// Measurements' uncertainties
		matrix::Matrix<float, Axis::Count, vtest::State::size>
		meas_h_xyz{}; // Observation matrix where the rows correspond to the x,y,z observations and the columns to the state
	};

	// enum SensorFusionMask : uint8_t {
	// 	// Bit locations for fusion_mode
	// 	NO_SENSOR_FUSION    = 0,
	// 	USE_TARGET_GPS_POS  = (1 << 0),    ///< set to true to use target GPS position data
	// 	USE_UAV_GPS_VEL     = (1 << 1),    ///< set to true to use drone GPS velocity data
	// 	USE_EXT_VIS_POS 	= (1 << 2),    ///< set to true to use target external vision-based relative position data
	// 	USE_MISSION_POS     = (1 << 3),    ///< set to true to use the PX4 mission position
	// 	USE_TARGET_GPS_VEL  = (1 << 4),		///< set to true to use target GPS velocity data. Only for moving targets.
	// 	USE_UWB = (1 << 5) ///< set to true to use UWB.
	// };

	enum ObsValidMask : uint8_t {
		// Bit locations for valid observations
		NO_VALID_DATA 	     = 0,
		FUSE_TARGET_GPS_POS  = (1 << 0), ///< set to true if target GPS position data is ready to be fused
		FUSE_UAV_GPS_VEL     = (1 << 1), ///< set to true if drone GPS velocity data (and target GPS velocity data if the target is moving)
		FUSE_VISION     = (1 << 2), ///< set to true if target external vision-based relative position data is ready to be fused
		FUSE_MISSION_POS     = (1 << 3), ///< set to true if the PX4 mission position is ready to be fused
		FUSE_TARGET_GPS_VEL  = (1 << 4), ///< set to true if target GPS velocity data is ready to be fused
		FUSE_UWB 	     = (1 << 5)  ///< set to true if UWB data is ready to be fused
	};

	bool createEstimators();
	bool initEstimator(const matrix::Matrix <float, Axis::Count, vtest::State::size>
			   &state_init);
	bool updateStep(const matrix::Vector3f &vehicle_acc_ned);
	void predictionStep(const matrix::Vector3f &acc);

	void updateTargetGpsVelocity(const target_gnss_s &target_GNSS_report);

	inline bool hasNewNonGpsPositionSensorData(const ObsValidMask &vte_fusion_aid_mask) const
	{
		return (vte_fusion_aid_mask & ObsValidMask::FUSE_VISION)
		       || (vte_fusion_aid_mask & ObsValidMask::FUSE_UWB);
	}

	inline bool hasNewPositionSensorData(const ObsValidMask &vte_fusion_aid_mask) const
	{
		return vte_fusion_aid_mask & (ObsValidMask::FUSE_MISSION_POS |
					      ObsValidMask::FUSE_TARGET_GPS_POS |
					      ObsValidMask::FUSE_VISION |
					      ObsValidMask::FUSE_UWB);
	}

	// Only estimate the GNSS bias if we have a GNSS estimation and a secondary source of position
	inline bool shouldSetBias(const ObsValidMask &vte_fusion_aid_mask)
	{
		return isMeasValid(_pos_rel_gnss.timestamp) && hasNewNonGpsPositionSensorData(vte_fusion_aid_mask);
	};

	bool initializeEstimator(const ObsValidMask &vte_fusion_aid_mask,
				 const targetObs observations[ObsType::Type_count]);
	void updateBias(const ObsValidMask &vte_fusion_aid_mask,
			const targetObs observations[ObsType::Type_count]);
	void getPosInit(const ObsValidMask &vte_fusion_aid_mask,
			const targetObs observations[ObsType::Type_count], matrix::Vector3f &pos_init);
	bool fuseNewSensorData(const matrix::Vector3f &vehicle_acc_ned, ObsValidMask &vte_fusion_aid_mask,
			       const targetObs observations[ObsType::Type_count]);
	void processObservations(ObsValidMask &vte_fusion_aid_mask,
				 targetObs observations[ObsType::Type_count]);

	bool isLatLonAltValid(double lat_deg, double lon_deg, float alt_m, const char *who = nullptr) const;

	/* Vision data */
	void handleVisionData(ObsValidMask &vte_fusion_aid_mask, targetObs &obs_fiducial_marker);
	bool isVisionDataValid(const fiducial_marker_pos_report_s &fiducial_marker_pose);
	bool processObsVision(const fiducial_marker_pos_report_s &fiducial_marker_pose, targetObs &obs);

	/* UWB data */
	void handleUwbData(ObsValidMask &vte_fusion_aid_mask, targetObs &obs_uwb);
	bool isUwbDataValid(const sensor_uwb_s &uwb_report);
	bool processObsUwb(const sensor_uwb_s &uwb_report, targetObs &obs);

	/* UAV GPS data */
	void handleUavGpsData(ObsValidMask &vte_fusion_aid_mask,
			      targetObs &obs_gps_pos_mission,
			      targetObs &obs_gps_vel_uav);
	bool updateUavGpsData();
	bool isUavGpsPositionValid();
	bool isUavGpsVelocityValid();
	bool processObsGNSSPosMission(targetObs &obs);
	bool processObsGNSSVelUav(targetObs &obs);

	/* Target GPS data */
	void handleTargetGpsData(ObsValidMask &vte_fusion_aid_mask,
				 targetObs &obs_gps_pos_target,
				 targetObs &obs_gps_vel_target);
	bool isTargetGpsPositionValid(const target_gnss_s &target_GNSS_report);
	bool isTargetGpsVelocityValid(const target_gnss_s &target_GNSS_report);
	bool processObsGNSSPosTarget(const target_gnss_s &target_GNSS_report, targetObs &obs);
#if defined(CONFIG_VTEST_MOVING)
	bool processObsGNSSVelTarget(const target_gnss_s &target_GNSS_report, targetObs &obs);
#endif // CONFIG_VTEST_MOVING

	bool fuseMeas(const matrix::Vector3f &vehicle_acc_ned, const targetObs &target_pos_obs);
	void publishTarget();
	void publishInnov(const estimator_aid_source3d_s &target_innov, const ObsType type);

	uORB::Subscription _vehicle_gps_position_sub{ORB_ID(vehicle_gps_position)};
	uORB::Subscription _vehicle_attitude_sub{ORB_ID(vehicle_attitude)};
	uORB::Subscription _fiducial_marker_report_sub{ORB_ID(fiducial_marker_pos_report)};
	uORB::Subscription _target_gnss_sub{ORB_ID(target_gnss)};
	uORB::Subscription _sensor_uwb_sub{ORB_ID(sensor_uwb)};

	perf_counter_t _vte_predict_perf{perf_alloc(PC_ELAPSED, MODULE_NAME": VTE prediction")};
	perf_counter_t _vte_update_perf{perf_alloc(PC_ELAPSED, MODULE_NAME": VTE update")};

	struct rangeSensor {
		hrt_abstime timestamp = 0;
		bool valid = false;
		float dist_bottom = 0.f;
	};

	rangeSensor _range_sensor{};

	struct globalPos {
		hrt_abstime timestamp = 0;
		bool valid = false;
		double lat_deg = 0.0; 	// Latitude in degrees
		double lon_deg	= 0.0; 	// Longitude in degrees
		float alt_m = 0.f;	// Altitude in meters AMSL
		float eph = 0.f;
		float epv = 0.f;
	};

	globalPos _mission_land_position{};
	globalPos _uav_gps_position{};
	vehicle_attitude_s _vehicle_attitude{};

	struct velStamped {
		hrt_abstime timestamp = 0;
		bool valid = false;
		matrix::Vector3f xyz{};
		float uncertainty = 0.f;
	};

	velStamped _uav_gps_vel{};

	struct vecStamped {
		hrt_abstime timestamp = 0;
		bool valid = false;
		matrix::Vector3f xyz{};
	};

	vecStamped _local_position{};
	vecStamped _local_velocity{};
	vecStamped _target_gps_vel{};
	vecStamped _pos_rel_gnss{};
	vecStamped _velocity_offset_ned{};
	vecStamped _gps_pos_offset_ned{};
	bool _gps_pos_is_offset{false};
	bool _bias_set{false};

	uint64_t _last_vision_obs_fused_time{0};
	bool _estimator_initialized{false};

	KF_position *_target_est_pos[Axis::Count] {nullptr, nullptr, nullptr};

	hrt_abstime _last_predict{0}; // timestamp of last filter prediction
	hrt_abstime _last_update{0}; // timestamp of last filter update (used to check timeout)

	/* parameters from vision_target_estimator_params.c*/
	void checkMeasurementInputs();

	uint32_t _vte_TIMEOUT_US = 3_s;
	int _vte_aid_mask{0};
	float _target_acc_unc{0.f};
	float _bias_unc{0.f};
	float _uav_acc_unc{0.f};
	float _gps_vel_noise{0.f};
	float _gps_pos_noise{0.f};
	bool  _ev_noise_md{false};
	float _ev_pos_noise{0.f};
	float _nis_threshold{0.f};

	DEFINE_PARAMETERS(
		(ParamFloat<px4::params::VTE_ACC_D_UNC>) _param_vte_acc_d_unc,
		(ParamFloat<px4::params::VTE_ACC_T_UNC>) _param_vte_acc_t_unc,
		(ParamFloat<px4::params::VTE_BIAS_LIM>) _param_vte_bias_lim,
		(ParamFloat<px4::params::VTE_BIAS_UNC>) _param_vte_bias_unc,
		(ParamFloat<px4::params::VTE_POS_UNC_IN>) _param_vte_pos_unc_in,
		(ParamFloat<px4::params::VTE_VEL_UNC_IN>) _param_vte_vel_unc_in,
		(ParamFloat<px4::params::VTE_BIA_UNC_IN>) _param_vte_bias_unc_in,
		(ParamFloat<px4::params::VTE_ACC_UNC_IN>) _param_vte_acc_unc_in,
		(ParamFloat<px4::params::VTE_GPS_V_NOISE>) _param_vte_gps_vel_noise,
		(ParamFloat<px4::params::VTE_GPS_P_NOISE>) _param_vte_gps_pos_noise,
		(ParamInt<px4::params::VTE_EV_NOISE_MD>) _param_vte_ev_noise_md,
		(ParamFloat<px4::params::VTE_EVP_NOISE>) _param_vte_ev_pos_noise,
		(ParamInt<px4::params::VTE_EKF_AID>) _param_vte_ekf_aid,
		(ParamFloat<px4::params::VTE_MOVING_T_MAX>) _param_vte_moving_t_max,
		(ParamFloat<px4::params::VTE_MOVING_T_MIN>) _param_vte_moving_t_min,
		(ParamFloat<px4::params::VTE_POS_NIS_THRE>) _param_vte_pos_nis_thre
	)
};
} // namespace vision_target_estimator
