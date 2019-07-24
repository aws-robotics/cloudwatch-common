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
  MetricDatum metric_datum;

  void SetUp() override {
    metric_datum = MetricDatum();
  }

  void TearDown() override {

  }

};

TEST_F(TestMetricSerialization, deserialize_returns_metric_datum) {
  const Aws::String mock_serialized_metric_datum{
      R"({"timestamp": 5, "metric_name": "awesomeness", "counts": [{"count": 123}, {"count": 456}], "storage_resolution": 25, "unit": 1})"
  };

  MetricDatum result;
  EXPECT_NO_THROW(result = deserializeMetricDatum(mock_serialized_metric_datum));
  Aws::Utils::DateTime ts((int64_t)5);
  EXPECT_EQ(result.GetTimestamp(), ts);
  EXPECT_EQ(result.GetMetricName(), "awesomeness");
  auto expected_counts = Aws::Vector<double>();
  expected_counts.push_back(123);
  expected_counts.push_back(456);
  EXPECT_EQ(result.GetCounts(), expected_counts);
  EXPECT_EQ(result.GetStorageResolution(), 25);
  EXPECT_EQ(result.GetUnit(), Aws::CloudWatch::Model::StandardUnit::Seconds);
}

TEST_F(TestMetricSerialization, deserialize_works_with_minimum_data) {
  const Aws::String min_json = R"({"metric_name": "minimum", "timestamp": 10})";
  MetricDatum result;
  EXPECT_NO_THROW(result = deserializeMetricDatum(min_json));
  EXPECT_EQ(result.GetMetricName(), "minimum");
}

TEST_F(TestMetricSerialization, deserialize_fail_throws_runtime_exception) {
  const Aws::String bad_json = "not json";
  EXPECT_THROW(deserializeMetricDatum(bad_json), std::invalid_argument);
  const Aws::String json_missing_values = "{}";
  EXPECT_THROW(deserializeMetricDatum(json_missing_values), std::invalid_argument);
}

TEST_F(TestMetricSerialization, serialize_returns_valid_string) {
  const Aws::Utils::DateTime ts = Aws::Utils::DateTime::Now();
  const Aws::String metric_name("cerealized");
  const Aws::Vector<double> counts = {111, 222};
  const int storage_resolution = 25;
  const auto metric_unit = Aws::CloudWatch::Model::StandardUnit::Seconds;
  const double value = 42;

  metric_datum.SetTimestamp(ts);
  metric_datum.SetMetricName(metric_name);
  metric_datum.SetCounts(counts);
  metric_datum.SetStorageResolution(storage_resolution);
  metric_datum.SetUnit(metric_unit);
  metric_datum.SetValue(value);
  Aws::String serialized_metric_datum;
  EXPECT_NO_THROW(serialized_metric_datum = serializeMetricDatum(metric_datum));
  MetricDatum result;
  EXPECT_NO_THROW(result = deserializeMetricDatum(serialized_metric_datum));
  EXPECT_EQ(result.GetTimestamp().Millis(), ts.Millis());
  EXPECT_EQ(result.GetMetricName(), metric_name);
  EXPECT_EQ(result.GetCounts(), counts);
  EXPECT_EQ(result.GetStorageResolution(), storage_resolution);
  EXPECT_EQ(result.GetUnit(), metric_unit);
  EXPECT_EQ(result.GetValue(), value);
}

// disabled as statistics currently not supported
TEST_F(TestMetricSerialization, DISABLED_statistic_values_work) {
  auto statistic_values = Aws::CloudWatch::Model::StatisticSet();
  statistic_values.SetMinimum(5);
  statistic_values.SetMaximum(15);
  statistic_values.SetSampleCount(3);
  statistic_values.SetSum(30);

  metric_datum.SetStatisticValues(statistic_values);
  Aws::String serialized_metric_datum =  serializeMetricDatum(metric_datum);
  MetricDatum result = deserializeMetricDatum(serialized_metric_datum);

  EXPECT_EQ(result.GetStatisticValues().GetMinimum(), statistic_values.GetMinimum());
  EXPECT_EQ(result.GetStatisticValues().GetMaximum(), statistic_values.GetMaximum());
  EXPECT_EQ(result.GetStatisticValues().GetSampleCount(), statistic_values.GetSampleCount());
  EXPECT_EQ(result.GetStatisticValues().GetSum(), statistic_values.GetSum());
}

// disabled as values currently not supported
TEST_F(TestMetricSerialization, DISABLED_values_work) {
  const Aws::Vector<double> values = {44, 55, 66};

  metric_datum.SetValues(values);
  Aws::String serialized_metric_datum =  serializeMetricDatum(metric_datum);
  MetricDatum result = deserializeMetricDatum(serialized_metric_datum);

  EXPECT_EQ(result.GetValues(), values);
}

TEST_F(TestMetricSerialization, dimensions_work) {
  auto d1 = Dimension();
  d1.SetName("d1");
  d1.SetValue("d1-value");
  auto d2 = Dimension();
  d2.SetName("d2");
  d2.SetValue("d2-value");
  const Aws::Vector<Dimension> dimensions = {d1, d2};

  metric_datum.SetDimensions(dimensions);
  Aws::String serialized_metric_datum =  serializeMetricDatum(metric_datum);
  MetricDatum result = deserializeMetricDatum(serialized_metric_datum);

  EXPECT_EQ(result.GetDimensions()[0].GetName(), d1.GetName());
  EXPECT_EQ(result.GetDimensions()[0].GetValue(), d1.GetValue());
  EXPECT_EQ(result.GetDimensions()[1].GetName(), d2.GetName());
  EXPECT_EQ(result.GetDimensions()[1].GetValue(), d2.GetValue());
}