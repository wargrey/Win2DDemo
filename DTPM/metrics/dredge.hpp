#pragma once

#include "graphlet/ui/metricslet.hpp"

#include "compass.hpp"
#include "plc.hpp"

namespace WarGrey::DTPM {
	private class DredgeMetrics : public virtual WarGrey::DTPM::IMetricsProvider {
	public:
		DredgeMetrics(WarGrey::DTPM::Compass* compass, WarGrey::SCADA::MRMaster* plc);

	public:
		unsigned int capacity() override;

	public:
		Platform::String^ label_ref(unsigned int idx) override;
		WarGrey::DTPM::MetricValue value_ref(unsigned int idx) override;
		
	public:
		Microsoft::Graphics::Canvas::Brushes::CanvasSolidColorBrush^ label_color_ref(unsigned int idx) override;

	private:
		~DredgeMetrics() noexcept {};

	private:
		class Provider;
		WarGrey::DTPM::DredgeMetrics::Provider* provider;
	};
}
