# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- iOS support (in development)

### Changed
- Improved error handling and memory management

## [0.0.6] 

### Added
- Enhanced WebP compression support
- Improved memory management for large images
- Better error handling for unsupported file formats
- Thread-safe operations with proper synchronization

### Changed
- Updated FFI bindings for better performance
- Improved temporary file management
- Enhanced format detection logic

### Fixed
- Memory leaks in image processing pipeline
- File path handling issues on Android
- Quality parameter validation

## [0.0.5] 

### Added
- Support for multiple image formats (JPEG, WebP, AUTO)
- Image information retrieval without compression
- Size-constrained compression with format support
- Quality control from 1-100

### Changed
- Refactored compression pipeline for better performance
- Updated VIPS library integration

## [0.0.4] 

### Added
- Basic image compression functionality
- JPEG format support
- Quality parameter support
- Temporary file output

### Changed
- Initial FFI implementation
- Basic Android platform support

## [0.0.3] 

### Added
- Initial project structure
- VIPS library integration
- Basic FFI bindings

## [0.0.2] 

### Added
- Project setup and configuration
- Basic plugin structure

## [0.0.1] 

### Added
- Initial project creation
- Basic Flutter plugin structure
- README and documentation setup
