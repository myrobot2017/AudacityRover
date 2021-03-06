#pragma once

#include "SenseHATSensor.h"

namespace AudacityRover {
   class TemperatureSenseHAT : public AudacityRover::SenseHATSensor
   {
   public:
      TemperatureSenseHAT(OpenALRF::sensorid_t AIdentifier);

      bool NextValue(OpenALRF::Sensor3DData &AValue) override;
   };
};
