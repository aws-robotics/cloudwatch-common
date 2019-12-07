//
// Created by root on 6/19/19.
//

#include <aws_common/sdk_utils/aws_error.h>
#include <gtest/gtest.h>
#include <aws/core/utils/StringUtils.h>
#include <aws/monitoring/model/MetricDatum.h>
#include <cloudwatch_metrics_common/utils/metric_serialization.hpp>
#include <cloudwatch_metrics_common/definitions/definitions.h>

using namespace Aws::CloudWatchMetrics::Utils;
using namespace Aws::CloudWatch::Model;

class TestMetricSerialization : public ::testing::Test
{
protected:
  MetricDatum metric_datum_;

  void SetUp() override {
    metric_datum_ = MetricDatum();
  }

  void TearDown() override {
  }

};

TEST_F(TestMetricSerialization, deserialize_returns_metric_datum) {
  const Aws::String mock_serialized_metric_datum{
      R"({"timestamp": 5, "metric_name": "awesomeness", "storage_resolution": 25, "unit": 1})"
  };

  MetricDatum result;
  EXPECT_NO_THROW(result = DeserializeMetricDatum(mock_serialized_metric_datum));
  Aws::Utils::DateTime ts(static_cast<int64_t>(5));
  EXPECT_EQ(result.GetTimestamp(), ts);
  EXPECT_EQ(result.GetMetricName(), "awesomeness");
  auto expected_counts = Aws::Vector<double>();
  EXPECT_EQ(result.GetStorageResolution(), 25);
  EXPECT_EQ(result.GetUnit(), Aws::CloudWatch::Model::StandardUnit::Seconds);
}

TEST_F(TestMetricSerialization, deserialize_works_with_minimum_data) {
  const Aws::String min_json = R"({"metric_name": "minimum", "timestamp": 10})";
  MetricDatum result;
  EXPECT_NO_THROW(result = DeserializeMetricDatum(min_json));
  EXPECT_EQ(result.GetMetricName(), "minimum");
}

TEST_F(TestMetricSerialization, deserialize_fail_throws_runtime_exception) {
  const Aws::String bad_json = "not json";
  EXPECT_THROW(DeserializeMetricDatum(bad_json), std::invalid_argument);
  const Aws::String json_missing_values = "{}";
  EXPECT_THROW(DeserializeMetricDatum(json_missing_values), std::invalid_argument);
}

TEST_F(TestMetricSerialization, serialize_returns_valid_string) {
  const Aws::Utils::DateTime ts = Aws::Utils::DateTime::Now();
  const Aws::String metric_name("cerealized");
  const Aws::Vector<double> counts = {111, 222};
  const int storage_resolution = 25;
  const auto metric_unit = Aws::CloudWatch::Model::StandardUnit::Seconds;
  const double value = 42;

  metric_datum_.SetTimestamp(ts);
  metric_datum_.SetMetricName(metric_name);
  metric_datum_.SetStorageResolution(storage_resolution);
  metric_datum_.SetUnit(metric_unit);
  metric_datum_.SetValue(value);
  Aws::String serialized_metric_datum;
  EXPECT_NO_THROW(serialized_metric_datum = SerializeMetricDatum(metric_datum_));
  MetricDatum result;
  EXPECT_NO_THROW(result = DeserializeMetricDatum(serialized_metric_datum));
  EXPECT_EQ(result.GetTimestamp().Millis(), ts.Millis());
  EXPECT_EQ(result.GetMetricName(), metric_name);
  EXPECT_EQ(result.GetStorageResolution(), storage_resolution);
  EXPECT_EQ(result.GetUnit(), metric_unit);
  EXPECT_EQ(result.GetValue(), value);
}