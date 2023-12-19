#include <tiledwebmaps/http.h>
#include <catch2/catch_test_macros.hpp>
#include <xtensor/xarray.hpp>
#include <xtensor/xview.hpp>

TEST_CASE("tiledwebmaps::Http")
{
  std::shared_ptr<cosy::proj::Context> proj_context = std::make_shared<cosy::proj::Context>();

  {
    tiledwebmaps::XYZ tile_loader("https://wms.openstreetmap.fr/tms/1.0.0/bayonne_2016/{zoom}/{x}/{y}", tiledwebmaps::Layout::XYZ(proj_context));

    xti::vec2i tile_coord({519997, 383334});
    int zoom = 20;

    auto tile_image = tile_loader.load(tile_coord, zoom);
  }

  {
    tiledwebmaps::WMS tile_loader("https://imagery.tnris.org/server/rest/services/StratMap/StratMap21_NCCIR_CapArea_Brazos_Kerr/ImageServer/exportImage?f=image&bbox={bbox}&imageSR=102100&bboxSR=102100&size={size}", tiledwebmaps::Layout::XYZ(proj_context));

    xti::vec2i tile_coord({479274, 863078});
    int zoom = 21;

    auto tile_image = tile_loader.load(tile_coord, zoom);
  }
}

TEST_CASE("tiledwebmaps::Layout")
{
  std::shared_ptr<cosy::proj::Context> proj_context = std::make_shared<cosy::proj::Context>();

  tiledwebmaps::Layout layout = tiledwebmaps::Layout::XYZ(proj_context);

  xti::vec2i tile_coord({519997, 383334});
  int zoom = 20;

  REQUIRE(xt::abs(xt::mean(tile_coord - layout.pixel_to_tile(layout.tile_to_pixel(tile_coord, zoom), zoom)))() < 1e-6);
  REQUIRE(xt::abs(xt::mean(tile_coord - layout.crs_to_tile(layout.tile_to_crs(tile_coord, zoom), zoom)))() < 1e-6);
}
