#include "pronto_ros/ins_ros_handler.hpp"
#include <eigen3/Eigen/Geometry>
#include <tf2/buffer_core.h>
#include <tf2_ros/transform_listener.h>
// #include <tf2/convert.h>
// #include "tf2_eigen/tf2_eigen.hpp"
// #include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>
#include "pronto_ros/pronto_ros_conversions.hpp"

namespace pronto {


InsHandlerROS::InsHandlerROS(rclcpp::Node::SharedPtr nh) : nh_(nh)
{
    tf2::BufferCore tf_imu_to_body_buffer_;
    tf2_ros::TransformListener tf_imu_to_body_listener_(tf_imu_to_body_buffer_);

    const std::string ins_param_prefix = "ins.";
    std::string imu_frame = "imu";
    nh_->get_parameter_or(ins_param_prefix + "frame", imu_frame, imu_frame);
    std::string base_frame = "base";
    nh_->get_parameter_or("base_link_name", base_frame, base_frame);
    
    RCLCPP_INFO_STREAM(nh_->get_logger(), 
        "[InsHandlerROS] Name of base_link: " 
        << base_frame);
    Eigen::Isometry3d ins_to_body = Eigen::Isometry3d::Identity();
    while (rclcpp::ok()) {
        try {
            geometry_msgs::msg::TransformStamped temp_transform = tf_imu_to_body_buffer_.lookupTransform(base_frame, imu_frame, tf2::TimePointZero);
            // ins_to_body = tf2::transformToEigen(temp_transform.transform);
            ins_to_body = transfToEigen(temp_transform.transform);
            RCLCPP_INFO_STREAM(nh_->get_logger(), 
                "IMU (" << imu_frame << ") to base (" << base_frame 
                << ") transform: translation=(" << ins_to_body.translation().transpose()
                << "), rotation=(" << ins_to_body.rotation() << ")");
            break;
        } 
        catch (const tf2::TransformException& ex) {
            RCLCPP_ERROR_SKIPFIRST(nh_->get_logger(), "%s", ex.what());
            rclcpp::sleep_for(std::chrono::seconds(1));
        }
    }

    InsConfig cfg;

    if (!nh_->get_parameter(ins_param_prefix + "num_to_init", cfg.num_to_init)) {
        RCLCPP_WARN_STREAM(nh_->get_logger(),
        "[InsHandlerROS] Couldn't get param " << ins_param_prefix 
        << "num_to_init. Using default: " << cfg.num_to_init);
    } else {
        RCLCPP_INFO_STREAM(nh_->get_logger(), 
            "num_to_init: " << cfg.num_to_init);
    }

    if (!nh_->get_parameter(ins_param_prefix + "accel_bias_update_online", cfg.accel_bias_update_online)) {
        RCLCPP_WARN_STREAM(nh_->get_logger(), 
            "[InsHandlerROS] Couldn't get param" << nh_->get_namespace() << "/" 
            << ins_param_prefix << "accel_bias_update_online\". Using default: "
            << cfg.accel_bias_update_online);
    }
    if (!nh_->get_parameter(ins_param_prefix + "gyro_bias_update_online", cfg.gyro_bias_update_online)) {
        RCLCPP_WARN_STREAM(nh_->get_logger(), 
            "[InsHandlerROS] Couldn't get param " << nh_->get_namespace() << "/" << ins_param_prefix 
            << "gyro_bias_update_online\". Using default: "
            << cfg.gyro_bias_update_online);
    }
    if (!nh_->get_parameter(ins_param_prefix + "accel_bias_recalc_at_start", cfg.accel_bias_recalc_at_start)) {
        RCLCPP_WARN_STREAM(nh_->get_logger(), 
            "[InsHandlerROS] Couldn't get param " << nh_->get_namespace() << "/" << ins_param_prefix 
            << "accel_bias_recalc_at_start\". Using default: "
            << cfg.accel_bias_recalc_at_start);
    }
    if (!nh_->get_parameter(ins_param_prefix + "gyro_bias_recalc_at_start", cfg.gyro_bias_recalc_at_start)) {
        RCLCPP_WARN_STREAM(nh_->get_logger(), 
            "[InsHandlerROS] Couldn't get param " << nh_->get_namespace() << "/" << ins_param_prefix 
            << "gyro_bias_recalc_at_start\". Using default: "
            << cfg.gyro_bias_recalc_at_start);
    }
    if (!nh_->get_parameter(ins_param_prefix + "timestep_dt", cfg.dt)) {
        RCLCPP_WARN_STREAM(nh_->get_logger(), 
            "[InsHandlerROS] Couldn't get param " << nh_->get_namespace() << "/" << ins_param_prefix
            << "timestep_dt\". Using default: "
            << cfg.dt);
    }
    std::vector<double> accel_bias_initial_v;
    std::vector<double> gyro_bias_initial_v;
    if (!nh_->get_parameter(ins_param_prefix + "accel_bias_initial", accel_bias_initial_v)) {
        RCLCPP_WARN_STREAM(nh_->get_logger(), 
            "[InsHandlerROS] Couldn't get param " << nh_->get_namespace() << "/" << ins_param_prefix 
            << "accel_bias_initial\". Using default: "
            << cfg.accel_bias_initial.transpose());
    } else {
        cfg.accel_bias_initial = Eigen::Map<Eigen::Vector3d, Eigen::Unaligned>(accel_bias_initial_v.data());
        RCLCPP_INFO_STREAM(nh_->get_logger(), 
            "accel_bias_initial: " << cfg.accel_bias_initial.transpose());
    }

    if (!nh_->get_parameter(ins_param_prefix + "gyro_bias_initial", gyro_bias_initial_v)) {
        RCLCPP_WARN_STREAM(nh_->get_logger(), 
            "[InsHandlerROS] Couldn't get param " << nh_->get_namespace() << "/" << ins_param_prefix 
            << "gyro_bias_initial\". Using default: "
            << cfg.gyro_bias_initial.transpose());
    } else {
        cfg.gyro_bias_initial = Eigen::Map<Eigen::Vector3d, Eigen::Unaligned>(gyro_bias_initial_v.data());
        RCLCPP_INFO_STREAM(nh_->get_logger(), 
            "gyro_bias_initial: "
            << cfg.gyro_bias_initial.transpose());
    }

    if (!nh_->get_parameter(ins_param_prefix + "max_initial_gyro_bias", cfg.max_initial_gyro_bias)) {
        RCLCPP_WARN_STREAM(nh_->get_logger(), 
            "[InsHandlerROS] Couldn't get param " << nh_->get_namespace() << "/" << ins_param_prefix 
            << "max_initial_gyro_bias\". Using default: "
            << cfg.max_initial_gyro_bias);
    }

    if (!nh_->get_parameter(ins_param_prefix + "topic", imu_topic_)) {
        RCLCPP_WARN_STREAM(nh_->get_logger(), 
            "[InsHandlerROS] Couldn't get param " << nh_->get_namespace() << "/" << ins_param_prefix 
            << "topic\". Using default: "
            << imu_topic_);
    }
    
    if (!nh_->get_parameter(ins_param_prefix + "downsample_factor", downsample_factor_)) {
        RCLCPP_WARN_STREAM(nh_->get_logger(), 
            "[InsHandlerROS] Couldn't get param " << nh_->get_namespace() << "/" << ins_param_prefix 
            << "downsample_factor\". Using default: "
            << downsample_factor_);
    } 

    if (!nh_->get_parameter(ins_param_prefix + "roll_forward_on_receive", roll_forward_on_receive_)) {
        RCLCPP_WARN_STREAM(nh_->get_logger(), 
            "[InsHandlerROS] Couldn't get param " << nh_->get_namespace() << "/" << ins_param_prefix 
            << "roll_forward_on_receive\". Using default: "
            << roll_forward_on_receive_);
    }
    
    if (!nh_->get_parameter(ins_param_prefix + "utime_offset", utime_offset_)) {
        RCLCPP_WARN_STREAM(nh_->get_logger(), 
            "[InsHandlerROS] Couldn't get param " << nh_->get_namespace() << "/" << ins_param_prefix 
            << "utime_offset\". Using default: "
            << utime_offset_);
    } 

    double std_accel = 0;
    double std_gyro = 0;
    double std_gyro_bias = 0;
    double std_accel_bias = 0;

    if (!nh_->get_parameter(ins_param_prefix + "q_gyro", std_gyro)) {
        RCLCPP_WARN_STREAM(nh_->get_logger(), 
            "[InsHandlerROS] Couldn't get param " << nh_->get_namespace() << "/" << ins_param_prefix 
            << "q_gyro\". Using default: "
            << std_gyro);
    }
    if (!nh_->get_parameter(ins_param_prefix + "q_gyro_bias", std_gyro_bias)) {
        RCLCPP_WARN_STREAM(nh_->get_logger(), 
            "[InsHandlerROS] Couldn't get param " << nh_->get_namespace() << "/" << ins_param_prefix 
            << "q_gyro_bias\". Using default: "
            << std_gyro_bias);
    }
    if (!nh_->get_parameter(ins_param_prefix + "q_accel", std_accel)) {
        RCLCPP_WARN_STREAM(nh_->get_logger(), 
            "[InsHandlerROS] Couldn't get param " << nh_->get_namespace() << "/" << ins_param_prefix 
            << "q_accel\". Using default: "
            << std_accel);
    }
    if (!nh_->get_parameter(ins_param_prefix + "q_accel_bias", std_accel_bias)) {
        RCLCPP_WARN_STREAM(nh_->get_logger(), 
            "[InsHandlerROS] Couldn't get param " << nh_->get_namespace() << "/" << ins_param_prefix 
            << "q_accel_bias\". Using default: "
            << std_accel_bias);
    }

    cfg.cov_accel = std::pow(std_accel, 2);
    cfg.cov_gyro = std::pow(std_gyro * M_PI / 180.0, 2);
    cfg.cov_accel_bias = std::pow(std_accel_bias, 2);
    cfg.cov_gyro_bias = std::pow(std_gyro_bias * M_PI / 180.0, 2);

    ins_module_ = InsModule(cfg, ins_to_body);
}

RBISUpdateInterface* InsHandlerROS::processMessage(const sensor_msgs::msg::Imu *imu_msg, StateEstimator *est)
{
    // keep one every downsample_factor messages
    if (counter++ % downsample_factor_ != 0) {
        return nullptr;
    }
    msgToImuMeasurement(*imu_msg, imu_meas_, utime_offset_);
    return ins_module_.processMessage(&imu_meas_, est);
}

bool InsHandlerROS::processMessageInit(const sensor_msgs::msg::Imu *imu_msg,
    const std::map<std::string, bool> &sensor_initialized,
    const RBIS &default_state,
    const RBIM &default_cov,
    RBIS &init_state,
    RBIM &init_cov)
{
    msgToImuMeasurement(*imu_msg, imu_meas_);
    return ins_module_.processMessageInit(&imu_meas_,
        sensor_initialized,
        default_state,
        default_cov,
        init_state,
        init_cov);
}

} // namespace pronto
