﻿#include <map>

#include "metrics/times.hpp"

#include "graphlet/shapelet.hpp"
#include "graphlet/ui/textlet.hpp"

#include "datum/credit.hpp"
#include "datum/string.hpp"

#include "metrics.hpp"
#include "module.hpp"
#include "preference.hxx"
#include "brushes.hxx"

#include "configuration.hpp"
#include "plc.hpp"

#include "iotables/di_hopper_pumps.hpp"

using namespace WarGrey::SCADA;
using namespace WarGrey::DTPM;
using namespace WarGrey::GYDM;

using namespace Microsoft::Graphics::Canvas;
using namespace Microsoft::Graphics::Canvas::UI;
using namespace Microsoft::Graphics::Canvas::Text;
using namespace Microsoft::Graphics::Canvas::Brushes;

static CanvasSolidColorBrush^ region_background = Colours::make(diagnostics_region_background);
static CanvasSolidColorBrush^ metrics_background = Colours::make(diagnostics_alarm_background);
static CanvasSolidColorBrush^ metrics_foreground = Colours::Green;

static Platform::String^ dredging_open_timepoint_key = "Dredging_Open_UTC_Milliseconds";
static Platform::String^ dredging_close_timepoint_key = "Dredging_Close_UTC_Milliseconds";

namespace {
	private enum class T : unsigned int { BeginTime, EndTime, DredgingTime, _ };
}

class WarGrey::DTPM::TimeMetrics::Provider final : public PLCConfirmation {
public:
	Provider() {
		this->dredging_start = get_preference(dredging_open_timepoint_key, 0LL);
		this->dredging_end = get_preference(dredging_close_timepoint_key, 0LL);

		this->memory = global_resident_metrics();
		this->memory->tp.set(TP::DredgingStart, this->dredging_start);
		this->memory->tp.set(TP::DredgingEnd, this->dredging_end);
	}

public:
	void on_digital_input(long long timepoint_ms, const uint8* DB4, size_t count4, const uint8* DB205, size_t count205, Syslog* logger) override {
		bool hopper_on = (DI_hopper_pump_running(DB4, ps_hopper_pump_feedback) || DI_hopper_pump_running(DB4, sb_hopper_pump_feedback));

		if (hopper_on) {
			if ((this->dredging_start == 0LL) || ((this->dredging_end > 0LL) && (this->dredging_end < timepoint_ms))) {
				this->dredging_start = timepoint_ms;
				this->memory->tp.set(TP::DredgingStart, this->dredging_start);
				put_preference(dredging_open_timepoint_key, this->dredging_start);
			}

			if (this->dredging_end > 0LL) {
				this->dredging_end = 0LL;
				this->memory->tp.set(TP::DredgingEnd, 0LL);
				put_preference(dredging_close_timepoint_key, this->dredging_end);
			}
		} else {
			if (this->dredging_end == 0LL) {
				this->dredging_end = timepoint_ms;
				this->memory->tp.set(TP::DredgingEnd, this->dredging_end);
				put_preference(dredging_close_timepoint_key, this->dredging_end);
			}
		}

		this->current_timepoint = timepoint_ms;
	}

public:
	long long dredging_start;
	long long dredging_end;
	long long current_timepoint;

private: // never deletes these global objects manually
	ResidentMetrics* memory;
};

/*************************************************************************************************/
TimeMetrics::TimeMetrics(MRMaster* plc) {
	this->provider = new TimeMetrics::Provider();

	if (plc != nullptr) {
		plc->push_confirmation_receiver(this->provider);
	}
}

unsigned int TimeMetrics::capacity() {
	return _N(T);
}

Platform::String^ TimeMetrics::label_ref(unsigned int idx) {
	return _speak(_E(T, idx));
}

MetricValue TimeMetrics::value_ref(unsigned int idx) {
	MetricValue mv;

	mv.type = MetricValueType::Null;
	mv.as.fixnum = 0LL;

	switch (_E(T, idx)) {
	case T::DredgingTime: {
		mv.type = MetricValueType::Period;

		if (this->provider->dredging_start > 0LL) {
			long long end = ((this->provider->dredging_end > 0) ? this->provider->dredging_end : this->provider->current_timepoint);
			
			mv.as.fixnum = end - this->provider->dredging_start;
		}
	}; break;
	case T::BeginTime: {
		if (this->provider->dredging_start > 0) {
			mv.type = MetricValueType::Time;
			mv.as.fixnum = this->provider->dredging_start;
		}
	}; break;
	case T::EndTime: {
		if (provider->dredging_end > 0) {
			mv.type = MetricValueType::Time;
			mv.as.fixnum = this->provider->dredging_end;
		}
	}; break;
	}

	mv.as.fixnum /= 1000LL;

	return mv;
}

CanvasSolidColorBrush^ TimeMetrics::label_color_ref(unsigned int idx) {
	CanvasSolidColorBrush^ color = Colours::Foreground;

	return color;
}

/*************************************************************************************************/
Timepoint::Timepoint() : IASNSequence(_N(TP)) {}

void Timepoint::set(TP field, long long v) {
	if (field < TP::_) {
		this->seconds[_I(field)] = v;
	}
}

long long Timepoint::ref(TP field) {
	return ((field < TP::_) ? this->seconds[_I(field)] : 0LL);
}

Timepoint::Timepoint(const uint8* basn, size_t* offset) : Timepoint() {
	this->from_octets(basn, offset);
}

size_t Timepoint::field_payload_span(size_t idx) {
	return asn_fixnum_span(this->seconds[idx]);
}

size_t Timepoint::fill_field(size_t idx, uint8* octets, size_t offset) {
	return asn_fixnum_into_octets(this->seconds[idx], octets, offset);
}

void Timepoint::extract_field(size_t idx, const uint8* basn, size_t* offset) {
	this->seconds[idx] = asn_octets_to_fixnum(basn, offset);
}
