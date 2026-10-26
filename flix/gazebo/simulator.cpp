// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// 用于运行Arduino代码并仿真无人机的Gazebo插件

#include <functional>
#include <cmath>
#include <gazebo/gazebo.hh>
#include <gazebo/physics/physics.hh>
#include <gazebo/common/common.hh>
#include <gazebo/sensors/sensors.hh>
#include <gazebo/msgs/msgs.hh>
#include <ignition/math/Vector3.hh>
#include <ignition/math/Pose3.hh>
#include <iostream>
#include <fstream>

#include "Arduino.h"
#include "flix.h"

#include "console.ino"
#include "control.ino"
#include "estimate.ino"
#include "safety.ino"
#include "log.ino"
#include "filter.h"
#include "mavlink.ino"
#include "motors.ino"
#include "parameters.ino"
#include "power.ino"
#include "rc.ino"
#include "time.ino"

using ignition::math::Vector3d;
using namespace gazebo;
using namespace std;

class ModelFlix : public ModelPlugin {
private:
	physics::ModelPtr model;
	physics::LinkPtr body;
	sensors::ImuSensorPtr imu;
	event::ConnectionPtr updateConnection, resetConnection;
	transport::NodePtr nodeHandle;
	transport::PublisherPtr motorPub[4];
	LowPassFilter<Vector> accFilter = LowPassFilter<Vector>(0.1);

public:
	void Load(physics::ModelPtr _parent, sdf::ElementPtr /*_sdf*/) {
		this->model = _parent;
		this->body = this->model->GetLink("body");
		this->imu = dynamic_pointer_cast<sensors::ImuSensor>(sensors::get_sensor(model->GetScopedName(true) + "::body::imu")); // default::flix::body::imu
		this->updateConnection = event::Events::ConnectWorldUpdateBegin(std::bind(&ModelFlix::OnUpdate, this));
		this->resetConnection = event::Events::ConnectWorldReset(std::bind(&ModelFlix::OnReset, this));
		initNode();
		Serial.begin(0);
		setupParameters();
		rcRxPin = 1; // 设置遥控引脚以启用遥控读取
		gzmsg << "Flix插件已加载" << endl;
	}

	void OnReset() {
		attitude = Quaternion(); // 重置估计的姿态
		armed = false;
		__resetTime += __micros;
		gzmsg << "Flix插件已重置" << endl;
	}

	void OnUpdate() {
		__micros = model->GetWorld()->SimTime().Double() * 1000000;
		step();

		// 读取虚拟IMU
		gyro = Vector(imu->AngularVelocity().X(), imu->AngularVelocity().Y(), imu->AngularVelocity().Z());
		acc = this->accFilter.update(Vector(imu->LinearAcceleration().X(), imu->LinearAcceleration().Y(), imu->LinearAcceleration().Z()));

		voltage = 4.2f; // 虚拟电压值

		readRC();
		estimate();

		// 将偏航校正为实际偏航
		attitude.setYaw(this->model->WorldPose().Yaw());

		control();
		handleConsole();
		processMavlink();

		applyMotorForces();
		publishTopics();
		logData();
		syncParameters();
	}

	void applyMotorForces() {
		// 推力
		const double dist = 0.035355; // 电机相对中心的偏移距离，米
		const double maxThrust = 0.03 * ONE_G; // 约30克，https://youtu.be/VtKI4Pjx8Sk?&t=78

		const float scale0 = 1.0, scale1 = 1.1, scale2 = 0.9, scale3 = 1.05; // 模拟电机不对称
		float mfl = scale0 * maxThrust * motors[MOT_FL];
		float mfr = scale1 * maxThrust * motors[MOT_FR];
		float mrl = scale2 * maxThrust * motors[MOT_RL];
		float mrr = scale3 * maxThrust * motors[MOT_RR];

		body->AddLinkForce(Vector3d(0.0, 0.0, mfl), Vector3d(dist, dist, 0.0));
		body->AddLinkForce(Vector3d(0.0, 0.0, mfr), Vector3d(dist, -dist, 0.0));
		body->AddLinkForce(Vector3d(0.0, 0.0, mrl), Vector3d(-dist, dist, 0.0));
		body->AddLinkForce(Vector3d(0.0, 0.0, mrr), Vector3d(-dist, -dist, 0.0));

		// 力矩
		const double maxTorque = 0.0024 * ONE_G; // 约24克·厘米
		body->AddRelativeTorque(Vector3d(0.0, 0.0, scale0 * maxTorque * motors[MOT_FL]));
		body->AddRelativeTorque(Vector3d(0.0, 0.0, scale1 * -maxTorque * motors[MOT_FR]));
		body->AddRelativeTorque(Vector3d(0.0, 0.0, scale2 * -maxTorque * motors[MOT_RL]));
		body->AddRelativeTorque(Vector3d(0.0, 0.0, scale3 * maxTorque * motors[MOT_RR]));
	}

	void initNode() {
		nodeHandle = transport::NodePtr(new transport::Node());
		nodeHandle->Init();
		string ns = "~/" + model->GetName();
		// 创建电机输出主题，用于调试和绘图
		motorPub[0] = nodeHandle->Advertise<msgs::Int>(ns + "/motor0");
		motorPub[1] = nodeHandle->Advertise<msgs::Int>(ns + "/motor1");
		motorPub[2] = nodeHandle->Advertise<msgs::Int>(ns + "/motor2");
		motorPub[3] = nodeHandle->Advertise<msgs::Int>(ns + "/motor3");
	}

	void publishTopics() {
		for (int i = 0; i < 4; i++) {
			msgs::Int msg;
			msg.set_data(static_cast<int>(round(motors[i] * 1000)));
			motorPub[i]->Publish(msg);
		}
	}
};

GZ_REGISTER_MODEL_PLUGIN(ModelFlix)
